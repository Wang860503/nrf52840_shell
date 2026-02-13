/* Copyright 2021-2022 NXP
 *
 * NXP Confidential. This software is owned or controlled by NXP and may only
 * be used strictly in accordance with the applicable license terms.  By
 * expressly accepting such terms or by downloading, installing, activating
 * and/or otherwise using the software, you are agreeing that you have read,
 * and that you agree to comply with and are bound by, such license terms.  If
 * you do not agree to be bound by the applicable license terms, then you may
 * not retain, install, activate or otherwise use the software.
 */

// #include <QN9090.h>
#include <uwb_bus_interface.h>

#include "UWB_GPIOExtender.h"
#include "board.h"
#include "phNxpLogApis_UwbApi.h"

extern eRhodes4Revision gRhodesV4_Rev;

static uwbs_io_callback
    mCallbacks[FSL_FEATURE_PINT_NUMBER_OF_CONNECTED_OUTPUTS];

void uwb_bus_io_irq_cb(void* args) {
  uwb_bus_board_ctx_t* pCtx = (uwb_bus_board_ctx_t*)args;
  // Signal TML read task
  phOsalUwb_ProduceSemaphore(pCtx->mIrqWaitSem);
}

/* * [Zephyr Wrapper]
 * 這是 Zephyr GPIO ISR 的統一入口。
 * 它負責找出正確的 Context，然後呼叫 NXP 上層邏輯指定的 callback (target_cb)。
 */
static void zephyr_gpio_wrapper_uwb(const struct device* dev,
                                    struct gpio_callback* cb, uint32_t pins) {
  uwb_bus_board_ctx_t* pCtx =
      CONTAINER_OF(cb, uwb_bus_board_ctx_t, irq_cb_struct);

  /* 呼叫儲存的目標函式 */
  if (pCtx->target_cb_UWB.fn) {
    pCtx->target_cb_UWB.fn(pCtx->target_cb_UWB.args);
  }
}

static void zephyr_gpio_wrapper_venus(const struct device* dev,
                                      struct gpio_callback* cb, uint32_t pins) {
  uwb_bus_board_ctx_t* pCtx =
      CONTAINER_OF(cb, uwb_bus_board_ctx_t, venus_cb_struct);

  if (pCtx->target_cb_VENUS.fn) {
    pCtx->target_cb_VENUS.fn(pCtx->target_cb_VENUS.args);
  }
}

uwb_bus_status_t uwb_bus_io_init(uwb_bus_board_ctx_t* pCtx) {
  LOG_D("%s:enter", __FUNCTION__);
  if (pCtx == NULL) {
    LOG_ERR("uwbs bus context is NULL");
    return kUWB_bus_Status_FAILED;
  }

  /* 1. 從 Device Tree 載入並設定 UWB IRQ */
  pCtx->gpio_irq = (struct gpio_dt_spec)GPIO_DT_SPEC_GET_OR(
      DT_ALIAS(uwbirqgpio), gpios, {0});

  /* 檢查 IRQ 腳位是否有效且硬體就緒 */
  if (pCtx->gpio_irq.port && device_is_ready(pCtx->gpio_irq.port)) {
    /* [關鍵] 設定為輸入模式 */
    gpio_pin_configure_dt(&pCtx->gpio_irq, GPIO_INPUT);
  } else {
    LOG_ERR("UWB IRQ pin not ready or not defined");
    return kUWB_bus_Status_FAILED;
  }

  /* 3. 初始化其他輸出腳位 (CE, Sync 等) */
  /* 建議也在這裡一併初始化，避免後續 io_val_set 操作未配置的腳位 */
  pCtx->gpio_ce =
      (struct gpio_dt_spec)GPIO_DT_SPEC_GET_OR(DT_ALIAS(uwbcegpio), gpios, {0});
  if (pCtx->gpio_ce.port && device_is_ready(pCtx->gpio_ce.port)) {
    /* 預設輸出為 Low (Inactive) */
    gpio_pin_configure_dt(&pCtx->gpio_ce, GPIO_OUTPUT_INACTIVE);
  }

  pCtx->gpio_sync = (struct gpio_dt_spec)GPIO_DT_SPEC_GET_OR(
      DT_ALIAS(uwbsyncgpio), gpios, {0});
  if (pCtx->gpio_sync.port && device_is_ready(pCtx->gpio_sync.port)) {
    /* 預設輸出為 Low (Inactive) */
    gpio_pin_configure_dt(&pCtx->gpio_sync, GPIO_OUTPUT_INACTIVE);
  }

  /* 4. 呼叫 Enable 流程 (註冊中斷 Callback) */
  uwb_bus_io_uwbs_irq_enable(pCtx);

  return kUWB_bus_Status_OK;
}

/* This api is needed only for PNP Firmware and SN110 */
uwb_bus_status_t uwb_bus_io_uwbs_irq_enable(uwb_bus_board_ctx_t* pCtx) {
  uwbs_io_callback callback;
  if (pCtx == NULL) {
    LOG_E("uwbs bus context is NULL");
    return kUWB_bus_Status_FAILED;
  }
  callback.fn = uwb_bus_io_irq_cb;
  callback.args = pCtx;
  return uwb_bus_io_irq_en(pCtx, kUWBS_IO_I_UWBS_IRQ, &callback);
}

uwb_bus_status_t uwb_bus_io_irq_en(uwb_bus_board_ctx_t* pCtx, uwbs_io_t irqPin,
                                   uwbs_io_callback* pCallback) {
  struct gpio_dt_spec* target_pin;
  struct gpio_callback* target_cb_struct;
  gpio_callback_handler_t wrapper_fn;
  uwbs_io_callback* storage_cb;
  gpio_flags_t trigger_flags;
  int ret;

  if (pCtx == NULL || pCallback == NULL) {
    LOG_ERR("Context or Callback is NULL");
    return kUWB_bus_Status_FAILED;
  }

  /* * 1. 根據 irqPin 選擇目標 GPIO 和觸發模式
   */
  switch (irqPin) {
    case kUWBS_IO_I_UWBS_IRQ:
      target_pin = &pCtx->gpio_irq;
      target_cb_struct = &pCtx->irq_cb_struct;
      storage_cb = &pCtx->target_cb_UWB;
      wrapper_fn = zephyr_gpio_wrapper_uwb;

      /* NXP Code: kPINT_PinIntEnableRiseEdge -> Rising Edge */
      /* 使用 EDGE_TO_ACTIVE 會自動參考 Device Tree 的 Active High/Low 設定 */
      trigger_flags = GPIO_INT_EDGE_TO_ACTIVE;
      break;

    case kUWBS_IO_I_VENUS_IRQ:
      target_pin = &pCtx->venus_pin;
      target_cb_struct = &pCtx->venus_cb_struct;
      storage_cb = &pCtx->target_cb_VENUS;
      wrapper_fn = zephyr_gpio_wrapper_venus;

      /* NXP Code: kPINT_PinIntEnableHighLevel -> Level High */
      trigger_flags = GPIO_INT_LEVEL_ACTIVE;
      break;

    default:
      LOG_ERR("Unsupported IRQ Pin: %d", irqPin);
      return kUWB_bus_Status_FAILED;
  }

  /* 檢查 Device Tree 是否有定義該腳位 */
  if (!target_pin->port) {
    LOG_ERR("GPIO pin not defined in Device Tree");
    return kUWB_bus_Status_FAILED;
  }

  /* * 2. 儲存 Callback
   * 將上層傳入的 fn 和 args 存起來，供 Wrapper 呼叫
   */
  storage_cb->fn = pCallback->fn;
  storage_cb->args = pCallback->args;

  /* * 3. 設定 GPIO 為輸入
   */
  if (!device_is_ready(target_pin->port)) {
    LOG_ERR("GPIO device not ready");
    return kUWB_bus_Status_FAILED;
  }
  gpio_pin_configure_dt(target_pin, GPIO_INPUT);

  /* * 4. 初始化並註冊 Zephyr Callback
   */
  gpio_init_callback(target_cb_struct, wrapper_fn, BIT(target_pin->pin));
  ret = gpio_add_callback(target_pin->port, target_cb_struct);
  if (ret < 0) {
    LOG_ERR("Failed to add callback");
    return kUWB_bus_Status_FAILED;
  }

  /* * 5. 啟用中斷
   * 這裡對應原本的 PINT_PinInterruptConfig + EnableCallback
   */
  ret = gpio_pin_interrupt_configure_dt(target_pin, trigger_flags);
  if (ret < 0) {
    LOG_ERR("Failed to enable interrupt");
    return kUWB_bus_Status_FAILED;
  }

  return kUWB_bus_Status_OK;
}

uwb_bus_status_t uwb_bus_io_val_set(uwb_bus_board_ctx_t* pCtx,
                                    uwbs_io_t gpioPin,
                                    uwbs_io_state_t gpioValue) {
  struct gpio_dt_spec* target_pin = NULL;
  int ret;

  if (pCtx == NULL) {
    LOG_E("uwbs bus context is NULL");
    return kUWB_bus_Status_FAILED;
  }

  /* 1. 選擇目標 Pin */
  switch (gpioPin) {
    case kUWBS_IO_O_HELIOS_SYNC:
      target_pin = &pCtx->gpio_sync;
      break;

    case kUWBS_IO_O_ENABLE_HELIOS:
      target_pin = &pCtx->gpio_ce;
      break;

    case kUWBS_IO_O_HELIOS_RTC_SYNC:
      /* Not needed, just return OK */
      return kUWB_bus_Status_OK;

    case kUWBS_IO_O_VENUS_VEN:
      return kUWB_bus_Status_OK;

    default:
      LOG_E("UWBD IO GPIO Pin not supported: %d", gpioPin);
      return kUWB_bus_Status_FAILED;
  }

  /* 2. 檢查 Pin 是否有效 (是否在 Device Tree 中有定義) */
  if (target_pin == NULL || target_pin->port == NULL) {
    /* 如果該腳位在 Device Tree 沒定義 (例如 VENUS
     * 在某些板子上沒有)，視為失敗或忽略 */
    LOG_W("GPIO pin not configured in Device Tree");
    return kUWB_bus_Status_FAILED;
  }

  /* 3. 執行寫入 (硬體抽象層)
   * 無論這是 SoC GPIO 還是 Expander GPIO，這個 API 都是一樣的。
   * 內部會自動處理 I2C 通訊 (如果是 Expander) 或暫存器寫入。
   */
  ret = gpio_pin_set_dt(target_pin, (int)gpioValue);

  if (ret < 0) {
    LOG_E("Failed to set GPIO value: %d", ret);
    return kUWB_bus_Status_FAILED;
  }

  return kUWB_bus_Status_OK;
}

uwb_bus_status_t uwb_bus_io_val_get(uwb_bus_board_ctx_t* pCtx,
                                    uwbs_io_t gpioPin,
                                    uwbs_io_state_t* pGpioValue) {
  const struct gpio_dt_spec* target_pin = NULL;
  int ret_val;

  /* 1. 基本參數檢查 */
  if (pCtx == NULL) {
    LOG_ERR("uwbs bus context is NULL");
    return kUWB_bus_Status_FAILED;
  }

  if (pGpioValue == NULL) {
    return kUWB_bus_Status_FAILED;
  }

  /* 2. 選擇目標 Pin (從 Context 中取得) */
  switch (gpioPin) {
    case kUWBS_IO_I_UWBS_IRQ:
      target_pin = &pCtx->gpio_irq; /* 之前定義為 irq_pin */
      break;

    case kUWBS_IO_O_HELIOS_SYNC:
      target_pin = &pCtx->gpio_sync;
      break;

    case kUWBS_IO_O_ENABLE_HELIOS:
      target_pin = &pCtx->gpio_ce;
      break;

    default:
      LOG_ERR("UWBD IO GPIO Pin not supported: %d", gpioPin);
      return kUWB_bus_Status_FAILED;
  }

  /* 3. 檢查 Pin 是否有效 */
  if (target_pin == NULL || target_pin->port == NULL) {
    /* 如果該腳位在 Device Tree 沒定義，視為 NA 或 Low */
    *pGpioValue = kUWBS_IO_State_NA; /* 或 kUWBS_IO_State_Low */
    /* 視情況決定是否回傳 FAILED，有些板子可能真的沒有 Venus */
    return kUWB_bus_Status_FAILED;
  }

  /* 4. 讀取 GPIO 狀態 (Zephyr 統一 API)
   *
   * 無論底層是透過 I2C (IO Expander) 還是直接存取暫存器，
   * gpio_pin_get_dt 都會處理好。
   */
  ret_val = gpio_pin_get_dt(target_pin);

  if (ret_val < 0) {
    LOG_ERR("Failed to read GPIO: %d", ret_val);
    *pGpioValue = kUWBS_IO_State_NA;
    return kUWB_bus_Status_FAILED;
  }

  /* 5. 轉換回傳值 */
  /* ret_val: 1 = High, 0 = Low */
  if (ret_val > 0) {
    *pGpioValue = kUWBS_IO_State_High;
  } else {
    *pGpioValue = kUWBS_IO_State_Low;
  }

  return kUWB_bus_Status_OK;
}

uwb_bus_status_t uwb_bus_io_irq_wait(uwb_bus_board_ctx_t* pCtx,
                                     uint32_t timeout_ms) {
  if (pCtx == NULL) {
    LOG_E("uwbs bus context is NULL");
    return kUWB_bus_Status_FAILED;
  }
  if (phOsalUwb_ConsumeSemaphore_WithTimeout(pCtx->mIrqWaitSem, timeout_ms) !=
      UWBSTATUS_SUCCESS) {
    LOG_D("phOsalUwb_ConsumeSemaphore_WithTimeout failed");
    return kUWB_bus_Status_FAILED;
  }
  return kUWB_bus_Status_OK;
}