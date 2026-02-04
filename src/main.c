/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Zephyr Shell on UART1 (RS485): TX=P0.19, RX=P0.20, P0.21=DE.
 * Shell TX DE is controlled by patch (HIGH before TX, LOW after); here only
 * boot message DE.
 */

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#define RS485_DE_PIN 21
#define RS485_DE_PORT DT_NODELABEL(gpio0)
#define DE_TX 1
#define DE_RX 0

static const struct device* const gpio0 = DEVICE_DT_GET(RS485_DE_PORT);

int main(void) {
  if (device_is_ready(gpio0)) {
    gpio_pin_configure(gpio0, RS485_DE_PIN, GPIO_OUTPUT_ACTIVE);
    gpio_pin_set(gpio0, RS485_DE_PIN, DE_TX);
    k_msleep(2);
  }
  printk("Senao Shell ready. Type 'help' for commands.\n");
  if (device_is_ready(gpio0)) {
    k_msleep(2);
    gpio_pin_set(gpio0, RS485_DE_PIN, DE_RX);
  }

  k_sleep(K_FOREVER);
  return 0;
}
