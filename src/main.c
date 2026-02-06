/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Zephyr Shell on UART1 (RS485): TX=P0.19, RX=P0.20, P0.21=DE.
 * Shell TX DE is controlled by patch (HIGH before TX, LOW after); here only
 * boot message DE.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "buzzer.h"

LOG_MODULE_REGISTER(my_app);

int main(void) {
  tone_powerup();
  LOG_INF("Senao Shell ready. Type 'help' for commands.\n");

  k_sleep(K_FOREVER);
  return 0;
}
