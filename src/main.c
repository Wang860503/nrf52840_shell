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
#include <zephyr/shell/shell.h>
#include <string.h>

#include "buzzer.h"
#include "radio_test.h"

LOG_MODULE_REGISTER(my_app);

#ifdef CONFIG_SHELL_THREAD_PRIORITY
/* 回調函數用於查找 shell 線程 */
static void find_shell_thread_cb(const struct k_thread* thread, void* user_data) {
  const char* thread_name = k_thread_name_get((k_tid_t)thread);
  if (thread_name != NULL && strcmp(thread_name, "shell_uart") == 0) {
    k_tid_t* found_tid = (k_tid_t*)user_data;
    *found_tid = (k_tid_t)thread;
  }
}
#endif

int main(void) {
  tone_powerup();
  clock_init();

  k_msleep(50);
  /* 使用 LOG 系統確保輸出 */
  printk("Senao Shell ready. Type 'help' for commands.\n");

  /* 再次延遲確保輸出完成 */
  k_msleep(50);

  /* 設置 shell 線程優先級
   * 使用 CONFIG_SHELL_THREAD_PRIORITY 配置值
   * 負數表示可搶占優先級（更高優先級），正數表示協作優先級
   */
#ifdef CONFIG_SHELL_THREAD_PRIORITY
  k_tid_t shell_tid = NULL;
  /* 使用 k_thread_foreach 遍歷所有線程查找 shell 線程 */
  k_thread_foreach(find_shell_thread_cb, &shell_tid);
  
  if (shell_tid != NULL) {
    k_thread_priority_set(shell_tid, CONFIG_SHELL_THREAD_PRIORITY);
    printk("Shell thread priority set to %d\n", CONFIG_SHELL_THREAD_PRIORITY);
  } else {
    printk("Warning: Shell thread not found, priority not set\n");
  }
#endif

  k_sleep(K_FOREVER);
  return 0;
}
