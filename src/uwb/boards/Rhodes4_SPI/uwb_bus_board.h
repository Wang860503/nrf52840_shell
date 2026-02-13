/* Copyright 2021 NXP
 *
 * NXP Confidential. This software is owned or controlled by NXP and may only
 * be used strictly in accordance with the applicable license terms.  By
 * expressly accepting such terms or by downloading, installing, activating
 * and/or otherwise using the software, you are agreeing that you have read,
 * and that you agree to comply with and are bound by, such license terms.  If
 * you do not agree to be bound by the applicable license terms, then you may
 * not retain, install, activate or otherwise use the software.
 */

#ifndef __UWB_BUS_BOARD_H__
#define __UWB_BUS_BOARD_H__

#include <uwb_uwbs_tml_io.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>

#ifndef HBCI_HEADER_SIZE
#define HBCI_HEADER_SIZE 4
#endif

#ifndef UWB_SPI_BAUDRATE
#define UWB_SPI_BAUDRATE (8 * 1000 * 1000U)  // 8 MHz
#endif

#ifndef UWB_SPI_NODE
#define UWB_SPI_NODE DT_ALIAS(uwbspi)
#endif

/** Board Specific BUS Interface for the Host HAL */
typedef struct {
  /* * [Zephyr 移植]
   * 取代原本的 masterHandle (SPI) 和 masterTx/RxHandle (DMA)。
   * 在 Zephyr 中，spi_dt_spec 包含了 SPI Bus 指標與 CS (Chip Select) 資訊。
   * DMA 則由底層 Driver 根據 Device Tree 自動處理。
   */
  struct spi_dt_spec spi;

  /* * [Zephyr 移植]
   * 建議將 GPIO 資訊也放在這裡，方便統一管理。
   */
  struct gpio_dt_spec gpio_irq; /* UWB INT Pin */
  struct gpio_callback irq_cb_struct;
  struct gpio_dt_spec gpio_reset; /* UWB RST Pin */
  struct gpio_dt_spec gpio_ce;    /* UWB Chip Enable Pin */
  struct gpio_dt_spec gpio_sync;
  struct gpio_dt_spec venus_pin;
  struct gpio_callback venus_cb_struct;

  /* [Zephyr] 用來儲存上層傳入的 Callback 函式指標 */
  uwbs_io_callback target_cb_UWB;
  uwbs_io_callback target_cb_VENUS;

  /* * [Zephyr 移植]
   * 取代 void *mIrqWaitSem。
   * 直接使用 Zephyr 的 semaphore 結構。
   */
  void* mIrqWaitSem;

} uwb_bus_board_ctx_t;

#endif  // __UWB_BUS_BOARD_H__