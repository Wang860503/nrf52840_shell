#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_uart.h>

LOG_MODULE_REGISTER(cmd);

static int cmd_switch_uart(const struct shell* sh, size_t argc, char** argv) {
  const struct device* new_dev;
  struct shell_uart_common* common = (struct shell_uart_common*)sh->iface->ctx;

  if (argc <= 1) {
    LOG_ERR("Usage: switch_uart <0|1>");
    return -EINVAL;
  }

  if (strcmp(argv[1], "0") == 0) {
    new_dev = DEVICE_DT_GET(DT_NODELABEL(uart0));
    LOG_INF("Switching to UART0...");
  } else if (strcmp(argv[1], "1") == 0) {
    new_dev = DEVICE_DT_GET(DT_NODELABEL(uart1));
    LOG_INF("Switching to UART1 (RS485)...");
  } else {
    LOG_ERR("Usage: switch_uart <0|1>");
    return -EINVAL;
  }

  if (common->dev == new_dev) {
    LOG_INF("Already active on %s. No action taken.", new_dev->name);
    return 0;
  }

  if (!device_is_ready(new_dev)) {
    LOG_ERR("UART device %s not ready!", new_dev->name);
    return -ENODEV;
  }

  k_msleep(50);
  int err = shell_init(sh, new_dev, (struct shell_backend_config_flags){0},
                       true, LOG_LEVEL_INF);
  LOG_INF("Shell init returned: %d\n", err);
  return err;
}

SHELL_CMD_REGISTER(switch_uart, NULL, "Switch Shell backend UART",
                   cmd_switch_uart);