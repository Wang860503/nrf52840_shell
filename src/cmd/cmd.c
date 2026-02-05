#include <string.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_uart.h>

LOG_MODULE_REGISTER(cmd);

static int cmd_switch_uart(const struct shell* sh, size_t argc, char** argv) {
  const struct device* new_dev;
  struct shell_uart_common* common = (struct shell_uart_common*)sh->iface->ctx;
  const struct shell_transport* transport = sh->iface;
  const struct device* old_dev = common->dev;

  if (argc <= 1) {
    shell_error(sh, "Usage: switch_uart <0|1>");
    return -EINVAL;
  }

  if (strcmp(argv[1], "0") == 0) {
    new_dev = DEVICE_DT_GET(DT_NODELABEL(uart0));
    shell_print(sh, "Switching to UART0...");
  } else if (strcmp(argv[1], "1") == 0) {
    new_dev = DEVICE_DT_GET(DT_NODELABEL(uart1));
    shell_print(sh, "Switching to UART1...");
  } else {
    shell_error(sh, "Usage: switch_uart <0|1>");
    return -EINVAL;
  }

  if (!device_is_ready(new_dev)) {
    shell_error(sh, "UART device %s not ready!", new_dev->name);
    return -ENODEV;
  }

  if (common->dev == new_dev) {
    shell_print(sh, "Already active on %s. No action taken.", new_dev->name);
    return 0;
  }

  shell_print(sh, "Switching from %s to %s...",
              old_dev ? old_dev->name : "NULL", new_dev->name);

  /* 等待輸出完成 */
  k_msleep(200);

  /* 保存必要信息 */
  shell_transport_handler_t evt_handler = common->handler;
  void* context = common->context;

  /* 先禁用舊 UART 的中斷，並清除回調函數（防止切換過程中的中斷） */
#if defined(CONFIG_SHELL_BACKEND_SERIAL_API_INTERRUPT_DRIVEN) && \
    defined(CONFIG_UART_INTERRUPT_DRIVEN)
  if (old_dev) {
    /* 禁用中斷 */
    uart_irq_tx_disable(old_dev);
    uart_irq_rx_disable(old_dev);
    /* 清除回調函數，防止舊設備的中斷在切換過程中觸發 */
    uart_irq_callback_user_data_set(old_dev, NULL, NULL);
    k_msleep(20); /* 等待所有待處理的中斷完成 */
  }
#endif

  /* 清理舊的 transport 配置 */
  if (transport && transport->api && transport->api->uninit) {
    transport->api->uninit(transport);
  }

  k_msleep(50);

  /* 通過 transport API 重新初始化到新的 UART */
  int err = 0;
  if (transport && transport->api && transport->api->init) {
    err = transport->api->init(transport, new_dev, evt_handler, context);
    if (err != 0) {
      LOG_ERR("Transport init failed: %d", err);
      return err;
    }
    k_msleep(100);

    /* 確保 RX 中斷已正確啟用 */
#if defined(CONFIG_SHELL_BACKEND_SERIAL_API_INTERRUPT_DRIVEN) && \
    defined(CONFIG_UART_INTERRUPT_DRIVEN)
    uart_irq_rx_disable(new_dev);
    uart_irq_tx_disable(new_dev);
    k_msleep(10);
    uart_irq_update(new_dev);
    k_msleep(10);
    uart_irq_rx_enable(new_dev);
    k_msleep(10);
    uart_irq_update(new_dev);
    k_msleep(10);
#endif
  } else {
    err = -ENOTSUP;
    LOG_ERR("Transport API not available");
    return err;
  }

  /* 清除可能的殘留數據 */
#if defined(CONFIG_SHELL_BACKEND_SERIAL_API_INTERRUPT_DRIVEN) && \
    defined(CONFIG_UART_INTERRUPT_DRIVEN)
  uint8_t dummy;
  while (uart_fifo_read(new_dev, &dummy, 1) > 0) {
    /* 清除 FIFO */
  }
#endif

  shell_print(sh, "Switched to %s successfully.", new_dev->name);
  return 0;
}

SHELL_CMD_REGISTER(switch_uart, NULL, "Switch Shell backend UART",
                   cmd_switch_uart);