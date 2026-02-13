/*!
 * *********************************************************************************
 * \addtogroup Private Profile Service
 * @{
 **********************************************************************************
 * */
/*!
 * *********************************************************************************
 * Copyright (c) 2014, Freescale Semiconductor, Inc.
 * All rights reserved.
 *
 * \file
 *
 * SPDX-License-Identifier: BSD-3-Clause
 **********************************************************************************
 * */

/************************************************************************************
*************************************************************************************
* Include
*************************************************************************************
************************************************************************************/
#include "private_profile_interface.h"
/************************************************************************************
*************************************************************************************
* Private constants & macros
*************************************************************************************
************************************************************************************/
LOG_MODULE_REGISTER(QPP_Adapter);
/************************************************************************************
*************************************************************************************
* Private type definitions
*************************************************************************************
************************************************************************************/

/************************************************************************************
*************************************************************************************
* Private memory declarations
*************************************************************************************
************************************************************************************/
uint8_t nearby_accessory_data[48];
extern struct bt_conn* current_conn;
/************************************************************************************
*************************************************************************************
* Public functions
*************************************************************************************
************************************************************************************/

bleResult_t Qpp_SendData(uint8_t deviceId, uint16_t serviceHandle,
                         uint16_t length, uint8_t* testData) {
  ARG_UNUSED(deviceId);      /* Zephyr 使用 current_conn */
  ARG_UNUSED(serviceHandle); /* NUS 不需要 Handle */

  int err;

  if (!current_conn) {
    LOG_WRN("Qpp_SendData: No connection");
    return gBleUnknown_c; /* 或是定義一個 ERROR code */
  }

  /* 使用 Nordic NUS 發送 Notification */
  err = bt_nus_send(current_conn, testData, length);

  if (err) {
    /* 常見錯誤：-EAGAIN (太快了Queue滿了), -ENOTCONN (沒連線/沒訂閱) */
    LOG_WRN("Qpp_SendData failed, err: %d", err);
    return gBleUnknown_c;
  }

  return gBleSuccess_c;
}

bleResult_t Update_Nearby(uint8_t deviceId, uint16_t size, uint8_t* testData) {
  ARG_UNUSED(deviceId);

  /* 1. 安全檢查，防止 Overflow */
  if (size > sizeof(nearby_accessory_data)) {
    LOG_WRN("Update_Nearby: Size %d too large, truncating to 48", size);
    size = sizeof(nearby_accessory_data);
  }

  /* 2. 直接更新記憶體 */
  memcpy(nearby_accessory_data, testData, size);

  LOG_DBG("Nearby Data Updated (%d bytes)", size);

  /* * 注意：NXP 原廠程式碼只呼叫 GattDb_WriteAttribute (寫入本地 DB)。
   * 如果手機端是透過 "Read" 來讀取，這樣就夠了。
   * 如果手機端是透過 "Notify" 來接收更新，這裡需要加 bt_gatt_notify。
   * 根據原廠 code，它只做 Write，所以這裡只 memcpy。
   */

  return gBleSuccess_c;
}

bleResult_t Erase_Nearby(uint8_t deviceId) {
  ARG_UNUSED(deviceId);

  /* 直接清空全域陣列 */
  memset(nearby_accessory_data, 0x00, sizeof(nearby_accessory_data));

  LOG_INF("Nearby Data Erased");

  return gBleSuccess_c;
}

bleResult_t Qpp_Subscribe(struct bt_conn* conn) {
  /* * 在 Zephyr 中，真正的 Subscribe 是由手機寫入 CCC 觸發
   * (on_qpp_tx_ccc_changed)。 這個函式主要用於應用層紀錄 "是誰連線了"。
   */
  current_conn = conn;
  return gBleSuccess_c;
}

bleResult_t Qpp_Unsubscribe(void) {
  current_conn = NULL;
  return gBleSuccess_c;
}
