/* TLV includes */
#include <zephyr/kernel.h>

#ifndef UWBIOT_APP_BUILD__DEMO_NEARBY_INTERACTION
#include "UWBIOT_APP_BUILD.h"
#endif

#ifdef UWBIOT_APP_BUILD__DEMO_NEARBY_INTERACTION

#include <stdbool.h>
#include <stdio.h>
#include <zephyr/logging/log.h>

#include "TLV_Types_i.h"
#include "phNxpLogApis_UwbApi.h"
#include "phOsalUwb_Queue.h"
#include "phUwbStatus.h"
#include "phUwbTypes.h"

extern intptr_t tlvMngQueue;
extern struct bt_conn* current_conn;
struct k_sem mHifSem;

#define MAX_HIF_TLV_WAIT (10)

static volatile UWB_Hif_t mInterface = UWB_HIF_BLE;

bool tlvSendRaw(uint8_t deviceId, uint8_t* buf, uint16_t size) {
  /* Send TLV */
  bool ok = TRUE;
  int err;

  if (mInterface == UWB_HIF_BLE) {
    /* 檢查連線是否存在 */
    if (!current_conn) {
      LOG_E("%s: No BLE connection established", __func__);
      return false;
    }

    /* * 1. 使用 Nordic NUS 發送數據
     * 注意：bt_nus_send 預設是非阻塞的 (放入 Queue)
     */
    err = bt_nus_send(current_conn, buf, size);

    /* * 2. 呼叫發送完成 Callback
     * 原廠邏輯是在這裡呼叫，通知上層 buffer 已經交給 Stack 了
     */
    tlvSendDoneCb();

    if (err) {
      LOG_E("%s: Failed to send over BLE (NUS), err: %d", __func__, err);
      ok = false;
      goto end;
    }
  } else {
    LOG_E("%s(): Invalid interface %X\n", __FUNCTION__, mInterface);
    ok = FALSE;
    goto end;
  }

  if (phOsalUwb_ConsumeSemaphore_WithTimeout(mHifSem, MAX_HIF_TLV_WAIT) !=
      UWBSTATUS_SUCCESS) {
    LOG_E("%s(): failed to wait HIF semaphore\n", __FUNCTION__);
    ok = FALSE;
  }

end:
  return ok;
}

void tlvSendDoneCb(void) { (void)phOsalUwb_ProduceSemaphore(mHifSem); }

void tlvRecv(uint8_t deviceId, UWB_Hif_t interface, uint8_t* tlv,
             uint8_t tlvSize) {
  mInterface = interface;
  static phLibUwb_Message_t shareable_config_buf = {0};
  shareable_config_buf.eMsgType = deviceId;
  shareable_config_buf.Size = tlvSize;
  shareable_config_buf.pMsgData = &tlv[0];

  int ret = phOsalUwb_msgsnd(tlvMngQueue, &shareable_config_buf, NO_DELAY);
  if (ret == UWBSTATUS_SUCCESS) {
    printk("%s success\n", __func__);
  } else {
    printk("%s fail\n", __func__);
  }
}

bool tlvBuilderInit(void) {
  if (phOsalUwb_CreateSemaphore(&mHifSem, 0) != UWBSTATUS_SUCCESS) {
    printk("Could not create TLV mutex");
    return FALSE;
  }
  return TRUE;
}

#endif