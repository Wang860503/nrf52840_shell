/*
 * Copyright (C) 2012-2022 NXP Semiconductors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* System includes */

#include <stdint.h>
#include <uwb_bus_board.h>
#include <uwb_bus_interface.h>

/* Freescale includes*/
#include "board.h"

/* UWB includes */
/* driver_config.h definitions are now in uwb_bus_board.h to avoid conflict with pn7160 */

#include "phNxpLogApis_UwbApi.h"
#include "phOsalUwb.h"
#include "phUwbErrorCodes.h"
#include "phUwbTypes.h"
#include "phUwb_BuildConfig.h"

/* This semaphore is signaled when SPI write is completed successfully*/
void* mSpiTransferSem = NULL;

uwb_bus_status_t uwb_bus_init(uwb_bus_board_ctx_t* pCtx) {
  if (pCtx == NULL) {
    LOG_ERR("uwbs bus context is NULL");
    return kUWB_bus_Status_FAILED;
  }

  /* * 2. SPI Initialization
   * 在 Zephyr 中，我們從 Device Tree 取得 SPI 規格。
   * 設定: Mode 0 (CPOL=0, CPHA=0), MSB First, 8-bit word.
   */

  /* 定義 SPI 操作模式: Master, 8-bit, Mode 0 */
  spi_operation_t operation =
      SPI_OP_MODE_MASTER | SPI_WORD_SET(8) | SPI_TRANSFER_MSB;
  /* 注意：Zephyr 若不設定 SPI_MODE_CPOL 和 SPI_MODE_CPHA，預設就是 Mode 0 */

  /* 初始化 spi_dt_spec 結構
   * UWB_SPI_NODE: 在 gpio_pins.h 定義的 DT_ALIAS(uwb_spi)
   * operation: 操作模式
   * 0: delay (通常填 0)
   */
  pCtx->spi = (struct spi_dt_spec)SPI_DT_SPEC_GET(UWB_SPI_NODE, operation, 0);

  /* 覆寫頻率 (從 gpio_pins.h 或 Device Tree 取得) */
  /* 注意：通常 SPI_DT_SPEC_GET 會自動抓 DT 裡的 spi-max-frequency，
   * 如果你想強制用 UWB_SPI_BAUDRATE，可以這樣改：
   */
  pCtx->spi.config.frequency = UWB_SPI_BAUDRATE;

  /* 檢查 SPI 裝置是否就緒 */
  if (!spi_is_ready_dt(&pCtx->spi)) {
    LOG_ERR("SPI device is not ready");
    return kUWB_bus_Status_FAILED;
  }

  /* * 3. DMA Initialization
   * Zephyr 不需要手動 DMA_Init 或 CreateHandle。
   * 只要在 .overlay 檔中設定了 "dmas" 屬性，SPI Driver 會自動處理。
   */

  /* * 4. Semaphore Creation
   */

  /* mIrqWaitSem: 用於 ISR 通知 Task */
  if (phOsalUwb_CreateSemaphore(&pCtx->mIrqWaitSem, 0) != UWBSTATUS_SUCCESS) {
    LOG_ERR("Error: could not create mIrqWaitSem");
    return kUWB_bus_Status_FAILED;
  }

  return kUWB_bus_Status_OK;
}

uwb_bus_status_t uwb_bus_deinit(uwb_bus_board_ctx_t* pCtx) {
  if (pCtx == NULL) {
    LOG_E("uwbs bus context is NULL");
    return kUWB_bus_Status_FAILED;
  }
  /* * [Zephyr Porting Note] SPI Deinit
   * Zephyr 的 SPI 驅動是系統層級管理的，應用層通常不需要顯式關閉 (Deinit)
   * 控制器。 如果您的目的是省電，應該使用電源管理 API (pm_device_action_run) 來
   * Suspend 裝置， 但這通常在 Idle Hook 或專門的 PM 邏輯中處理。
   * 因此，這裡直接忽略 SPI_Deinit 是安全的。
   */
  // SPI_Deinit(UWB_SPI_BASEADDR);

  /* 清理全域 Semaphore */
  if (mSpiTransferSem != NULL) {
    phOsalUwb_DeleteSemaphore(&mSpiTransferSem);
    mSpiTransferSem = NULL;
  }

  /* 清理 Context 內的 Semaphore */
  if (pCtx->mIrqWaitSem != NULL) {
    /* * 先發送訊號 (Produce) 以喚醒任何可能正在等待此 Semaphore 的執行緒，
     * 避免直接刪除導致 Deadlock 或存取錯誤。
     */
    phOsalUwb_ProduceSemaphore(pCtx->mIrqWaitSem);

    /* 讓出 CPU 一小段時間，確保被喚醒的執行緒有機會執行並退出 */
    phOsalUwb_Delay(2);

    phOsalUwb_DeleteSemaphore(&pCtx->mIrqWaitSem);
    pCtx->mIrqWaitSem = NULL;
  }

  /* 清除 Context 記憶體 (memset) */
  /* 注意：這會把包含 SPI/GPIO spec 在內的所有資料清空 */
  phOsalUwb_SetMemory(pCtx, 0, sizeof(uwb_bus_board_ctx_t));
  return kUWB_bus_Status_OK;
}
