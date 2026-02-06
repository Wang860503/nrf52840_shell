#include <stdlib.h>
#include <string.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_uart.h>

LOG_MODULE_REGISTER(cmd);

/* PCA9955B I2C 地址 (AD0-AD2 都接地 = 0x40) */
#define PCA9955B_I2C_ADDR 0x40

/* PCA9955B 寄存器定義 */
#define PCA9955B_REG_MODE1 0x00
#define PCA9955B_REG_MODE2 0x01
#define PCA9955B_REG_LEDOUT0 0x02
#define PCA9955B_REG_LEDOUT1 0x03
#define PCA9955B_REG_LEDOUT2 0x04
#define PCA9955B_REG_LEDOUT3 0x05
#define PCA9955B_REG_GRPPWM 0x06
#define PCA9955B_REG_GRPFREQ 0x07
#define PCA9955B_REG_PWM0 0x08
#define PCA9955B_REG_PWM15 0x17
#define PCA9955B_REG_IREF0 0x18
#define PCA9955B_REG_IREF15 0x27
#define PCA9955B_REG_IREFALL 0x45

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

/* PCA9955B 測試命令 */
/* 根據原理圖，使用的 LED 通道：
 * LED0  -> PWM0  (0x08) - White (Middle)
 * LED9  -> PWM9  (0x11) - White (Top)
 * LED11 -> PWM11 (0x13) - Green (Top)
 * LED12 -> PWM12 (0x14) - Red (Top)
 * LED15 -> PWM15 (0x17) - Red (Middle)
 */
static bool pca9955b_init = false;
static int cmd_pca9955b_test(const struct shell* sh, size_t argc, char** argv) {
  const struct device* i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c1));

  if (!device_is_ready(i2c_dev)) {
    shell_error(sh, "I2C device not ready!");
    return -ENODEV;
  }

  if (argc < 2) {
    shell_error(sh, "Usage: pca9955b_test <cmd> [args]");
    shell_print(sh, "Commands:");
    shell_print(sh, "  init          - Initialize PCA9955B");
    shell_print(sh,
                "  led <ch> <brightness> - Set LED channel (1,12,13,14,15) "
                "brightness (0-255)");
    shell_print(sh, "  all <brightness> - Set all LEDs brightness (0-255)");
    shell_print(sh, "  off            - Turn off all LEDs");
    shell_print(sh, "  demo           - Run LED demo");
    return -EINVAL;
  }

  if (strcmp(argv[1], "init") == 0) {
    shell_print(sh, "Initializing PCA9955B...");
    uint8_t buf[2];

    /* MODE1: 喚醒晶片 (SLEEP=0), 啟用 ALLCALL (0x01) */
    buf[0] = PCA9955B_REG_MODE1;
    buf[1] = 0x01;
    if (i2c_write(i2c_dev, buf, 2, PCA9955B_I2C_ADDR) != 0) {
      goto fail;
    }

    /* IREFALL: 設定全域輸出電流 (0x80 = 50% 電流，保護 LED) */
    buf[0] = PCA9955B_REG_IREFALL;
    buf[1] = 0x80;
    if (i2c_write(i2c_dev, buf, 2, PCA9955B_I2C_ADDR) != 0) {
      goto fail;
    }

    /* LEDOUT0-3: 設置所有通道為 PWM 模式 (0xAA) */
    for (uint8_t i = 0; i < 4; i++) {
      buf[0] = PCA9955B_REG_LEDOUT0 + i;
      buf[1] = 0xAA;
      if (i2c_write(i2c_dev, buf, 2, PCA9955B_I2C_ADDR) != 0) {
        goto fail;
      }
    }
    pca9955b_init = true;
    shell_print(sh, "PCA9955B initialized");
    return 0;
  fail:
    pca9955b_init = false;
    shell_error(sh, "Initializing fail!");
    return -ENXIO;
  }

  if (!pca9955b_init) {
    shell_error(sh, "Please init pca9955b");
    return -ECANCELED;
  }

  if (strcmp(argv[1], "led") == 0) {
    if (argc < 4) {
      shell_error(sh, "Usage: pca9955b_test led <ch> <brightness>");
      shell_print(sh, "  ch: 0, 9, 11, 12, 15");
      shell_print(sh, "  brightness: 0-255");
      return -EINVAL;
    }
    int ch = atoi(argv[2]);
    int brightness = atoi(argv[3]);

    if (brightness < 0) brightness = 0;
    if (brightness > 255) brightness = 255;

    uint8_t reg = PCA9955B_REG_PWM0 + ch;
    uint8_t buf[2] = {reg, (uint8_t)brightness};
    int ret = i2c_write(i2c_dev, buf, 2, PCA9955B_I2C_ADDR);
    if (ret == 0) {
      shell_print(sh, "LED%d set to brightness %d", ch, brightness);
    } else {
      shell_error(sh, "Write failed: %d", ret);
      return ret;
    }
    return 0;
  }

  if (strcmp(argv[1], "all") == 0) {
    if (argc < 3) {
      shell_error(sh, "Usage: pca9955b_test all <brightness>");
      return -EINVAL;
    }
    int brightness = atoi(argv[2]);
    if (brightness < 0) brightness = 0;
    if (brightness > 255) brightness = 255;

    uint8_t buf[2];
    int channels[] = {0, 9, 11, 12, 15};
    for (int i = 0; i < 5; i++) {
      buf[0] = PCA9955B_REG_PWM0 + channels[i];
      buf[1] = (uint8_t)brightness;
      i2c_write(i2c_dev, buf, 2, PCA9955B_I2C_ADDR);
    }
    shell_print(sh, "All LEDs set to brightness %d", brightness);
    return 0;
  }

  if (strcmp(argv[1], "off") == 0) {
    uint8_t buf[2];
    int channels[] = {0, 9, 11, 12, 15};
    for (int i = 0; i < 5; i++) {
      buf[0] = PCA9955B_REG_PWM0 + channels[i];
      buf[1] = 0;
      i2c_write(i2c_dev, buf, 2, PCA9955B_I2C_ADDR);
    }
    shell_print(sh, "All LEDs turned off");
    return 0;
  }

  if (strcmp(argv[1], "demo") == 0) {
    shell_print(sh, "Running LED demo...");
    uint8_t buf[2];

    /* 初始化 */
    buf[0] = PCA9955B_REG_MODE1;
    buf[1] = 0x00;
    i2c_write(i2c_dev, buf, 2, PCA9955B_I2C_ADDR);
    buf[0] = PCA9955B_REG_MODE2;
    buf[1] = 0x00;
    i2c_write(i2c_dev, buf, 2, PCA9955B_I2C_ADDR);
    buf[0] = PCA9955B_REG_LEDOUT0;
    buf[1] = 0xAA;
    i2c_write(i2c_dev, buf, 2, PCA9955B_I2C_ADDR);
    buf[0] = PCA9955B_REG_LEDOUT1;
    buf[1] = 0xAA;
    i2c_write(i2c_dev, buf, 2, PCA9955B_I2C_ADDR);
    buf[0] = PCA9955B_REG_LEDOUT2;
    buf[1] = 0xAA;
    i2c_write(i2c_dev, buf, 2, PCA9955B_I2C_ADDR);
    buf[0] = PCA9955B_REG_LEDOUT3;
    buf[1] = 0xAA;
    i2c_write(i2c_dev, buf, 2, PCA9955B_I2C_ADDR);
    k_msleep(100);

    /* 依次點亮每個 LED */
    int channels[] = {0, 9, 11, 12, 15};
    const char* names[] = {"White(Middle)", "White(Top)", "Green(Top)",
                           "Red(Top)", "Red(Middle)"};

    for (int i = 0; i < 5; i++) {
      shell_print(sh, "LED%d (%s)", channels[i], names[i]);
      buf[0] = PCA9955B_REG_PWM0 + channels[i];
      buf[1] = 255;
      i2c_write(i2c_dev, buf, 2, PCA9955B_I2C_ADDR);
      k_msleep(500);
      buf[1] = 0;
      i2c_write(i2c_dev, buf, 2, PCA9955B_I2C_ADDR);
      k_msleep(200);
    }

    /* 全部點亮 */
    shell_print(sh, "All LEDs ON");
    for (int i = 0; i < 5; i++) {
      buf[0] = PCA9955B_REG_PWM0 + channels[i];
      buf[1] = 128;
      i2c_write(i2c_dev, buf, 2, PCA9955B_I2C_ADDR);
    }
    k_msleep(1000);

    /* 關閉 */
    for (int i = 0; i < 5; i++) {
      buf[0] = PCA9955B_REG_PWM0 + channels[i];
      buf[1] = 0;
      i2c_write(i2c_dev, buf, 2, PCA9955B_I2C_ADDR);
    }
    shell_print(sh, "Demo complete");
    return 0;
  }

  shell_error(sh, "Unknown command: %s", argv[1]);
  return -EINVAL;
}

SHELL_CMD_REGISTER(switch_uart, NULL, "Switch Shell backend UART",
                   cmd_switch_uart);
SHELL_CMD_REGISTER(pca9955b_test, NULL, "PCA9955B LED driver test commands",
                   cmd_pca9955b_test);