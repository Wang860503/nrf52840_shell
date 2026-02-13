/******************************************************************************
 *
 *  Copyright (C) 1999-2012 Broadcom Corporation
 *  Copyright 2018-2019,2022,2023 NXP
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

#include "UwbAdaptation.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "UwbCoreSDK_Internal.h"
#include "phNxpLogApis_UwbApi.h"
#include "phNxpUciHal_Adaptation.h"
#include "phNxpUwbConfig.h"
#include "phOsalUwb.h"
#include "phUwb_BuildConfig.h"
#include "uwa_api.h"
#include "uwb_hal_int.h"
#include "uwb_int.h"
#include "uwb_target.h"

/* 定義 Stack 大小，建議給 4096 或 8192 以策安全 */
#define MY_UWB_TASK_STACK_SIZE 4096
/* 定義優先級 */
#define MY_UWB_TASK_PRIORITY 5

/* 1. 定義靜態 Stack 記憶體 (自動對齊，不會造成 Heap 損毀) */
K_THREAD_STACK_DEFINE(uwb_task_stack_area, MY_UWB_TASK_STACK_SIZE);

/* 2. 定義執行緒控制結構 */
struct k_thread uwb_task_thread_data;

static phUwbtask_Control_t uwb_ctrl;

static void HalOpen(tHAL_UWB_CBACK* p_hal_cback,
                    tHAL_UWB_DATA_CBACK* p_data_cback);
static void HalClose(void);
static void HalWrite(uint16_t data_len, uint8_t* p_data);
static tUCI_STATUS HalIoctl(long arg, tHAL_UWB_IOCTL* p_data);

#if (UWBIOT_UWBD_SR1XXT_SR2XXT)
static tUCI_STATUS HalApplyVendorConfigs(void);
static const tHAL_UWB_ENTRY mHalEntryFuncs = {HalOpen, HalClose, HalWrite,
                                              HalIoctl, HalApplyVendorConfigs};
#elif (UWBIOT_UWBD_SR040)
static const tHAL_UWB_ENTRY mHalEntryFuncs = {HalOpen, HalClose, HalWrite,
                                              HalIoctl, NULL};
#endif

#define MAX_TIMEOUT_UWB_TASK_SEM (50)
/*
uint32_t StartUwbTask() {
  phOsalUwb_ThreadCreationParams_t threadparams;

  threadparams.stackdepth = UWBTASK_STACK_SIZE;
  PHOSALUWB_SET_TASKNAME(threadparams, "UWB_TASK");
  threadparams.pContext = &uwb_ctrl;
  threadparams.priority = 5;

  if (phOsalUwb_Thread_Create((void**)&uwb_ctrl.task_handle, uwb_task,
                              &threadparams) != 0) {
    return UCI_STATUS_FAILED;
  }

  return UCI_STATUS_OK;
}
*/
uint32_t StartUwbTask() {
    /* 不再需要 threadparams 了，直接填入 Zephyr API */

    /* * k_thread_create 參數說明：
     * 1. 執行緒結構指標 (&uwb_task_thread_data)
     * 2. Stack 區域 (uwb_task_stack_area)
     * 3. Stack 大小 (K_THREAD_STACK_SIZEOF 巨集自動計算)
     * 4. 執行緒入口函式 (uwb_task)
     * 5. 參數1 (通常傳 Context, 這裡傳 &uwb_ctrl)
     * 6. 參數2 (NULL)
     * 7. 參數3 (NULL)
     * 8. 優先級 (MY_UWB_TASK_PRIORITY)
     * 9. 選項 (0)
     * 10. 啟動延遲 (K_NO_WAIT = 馬上啟動)
     */
    k_tid_t tid = k_thread_create(&uwb_task_thread_data, uwb_task_stack_area,
                                  K_THREAD_STACK_SIZEOF(uwb_task_stack_area),
                                  uwb_task, &uwb_ctrl, NULL, NULL,
                                  MY_UWB_TASK_PRIORITY, 0, K_NO_WAIT);

    /* 檢查是否建立成功 (雖然靜態建立幾乎不會失敗) */
    if (tid == NULL) {
        return UCI_STATUS_FAILED;
    }

    /* 保存 Handle (轉型一下符合 NXP 結構) */
    uwb_ctrl.task_handle = (void*)tid;

    k_usleep(100);

    return UCI_STATUS_OK;
}

extern phUwbtask_Control_t* gp_uwbtask_ctrl;

#define TIMER_1_EVT_MASK 0x0020

void phUwb_OSAL_send_msg(uint8_t task_id, uint16_t mbox, void* pmsg) {
    phLibUwb_Message_t msg;
    intptr_t pMsgQ = 0;

    switch (mbox) {
        case TIMER_1_EVT_MASK:
        case UWB_TASK_EVT_TRANSPORT_READY:
        case UWB_SHUTDOWN_EVT_MASK:
            msg.eMsgType = (uint16_t)mbox;
            break;
        default:
            msg.eMsgType = (uint16_t)EVENT_MASK(mbox);
    }

    msg.pMsgData = pmsg;
    msg.Size = 0;

    if (task_id == UWB_TASK) {
        /* 檢查 gp_uwbtask_ctrl 是否有效 */
        if (gp_uwbtask_ctrl == NULL) {
            LOG_W("%s : gp_uwbtask_ctrl is NULL, cannot send message", __FUNCTION__);
            return;
        }
        NXPLOG_UWBAPI_D("%s :pMsgQ pointer -> %p\n", __FUNCTION__,
                        gp_uwbtask_ctrl->pMsgQHandle);
        pMsgQ = gp_uwbtask_ctrl->pMsgQHandle;
    }

    if (pMsgQ != 0) {
        if (phOsalUwb_msgsnd(pMsgQ, &msg, NO_DELAY) != UWBSTATUS_SUCCESS) {
            LOG_W("%s : Failed to send message to queue %p", __FUNCTION__, (void*)pMsgQ);
        }
    } else {
        LOG_W("%s : Message queue is invalid (0), cannot send message", __FUNCTION__);
    }
}

/*******************************************************************************
**
** Function:    Initialize()
**
**
** Returns:     none
**
*******************************************************************************/
void Initialize() {
    NXPLOG_UWBAPI_D("%s :enter\n", __FUNCTION__);
    uint8_t mConfig;

    uwb_ctrl.pMsgQHandle = phOsalUwb_msgget(configTML_QUEUE_LENGTH);
    if (uwb_ctrl.pMsgQHandle == 0) {  // 或 NULL
        NXPLOG_UWBAPI_E("FATAL: Failed to allocate MsgQ! Heap full?");
        return;  // 必須中止初始化
    }
    (void)phOsalUwb_CreateSemaphore(&uwb_ctrl.uwb_task_sem, 0);
    NXPLOG_UWBAPI_D("%s :pMsgQHandle pointer -> %p\n", __FUNCTION__,
                    uwb_ctrl.pMsgQHandle);
    // gp_uwbtask_ctrl = &uwb_ctrl;
    if (StartUwbTask() == UCI_STATUS_OK) {
        (void)phNxpUciHal_GetNxpNumValue(UWB_FW_LOG_THREAD_ID, &mConfig,
                                         sizeof(mConfig));
    }
}

/*******************************************************************************
**
** Function:    Finalize()
**
** Returns:     none
**
*******************************************************************************/
void Finalize() {
    /* 檢查 UWB_TASK 線程是否還在運行 */
    if (uwb_ctrl.task_handle == NULL) {
        LOG_W("%s : UWB_TASK thread already deleted", __FUNCTION__);
        return;
    }
    
    /* 設置 gp_uwbtask_ctrl 以確保 phUwb_OSAL_send_msg 可以訪問消息隊列 */
    extern phUwbtask_Control_t* gp_uwbtask_ctrl;
    gp_uwbtask_ctrl = &uwb_ctrl;
    
    /* 嘗試發送關閉消息到 UWB_TASK（如果消息隊列還有效） */
    if (uwb_ctrl.pMsgQHandle != 0) {
        phUwb_OSAL_send_msg(UWB_TASK, UWB_SHUTDOWN_EVT_MASK, NULL);
        /* 等待 UWB_TASK 處理關閉消息，但不要等待太久 */
        if (uwb_ctrl.uwb_task_sem != NULL) {
            if (UWBSTATUS_SUCCESS !=
                phOsalUwb_ConsumeSemaphore_WithTimeout(uwb_ctrl.uwb_task_sem,
                                                       MAX_TIMEOUT_UWB_TASK_SEM)) {
                LOG_W("%s : phOsalUwb_ConsumeSemaphore_WithTimeout failed, thread may already be stopped",
                      __FUNCTION__);
            }
        }
    }
    
    /* 清理資源 */
    if (uwb_ctrl.uwb_task_sem != NULL) {
        phOsalUwb_DeleteSemaphore(&uwb_ctrl.uwb_task_sem);
        uwb_ctrl.uwb_task_sem = NULL;
    }
    if (uwb_ctrl.pMsgQHandle != 0) {
        phOsalUwb_msgrelease(uwb_ctrl.pMsgQHandle);
        uwb_ctrl.pMsgQHandle = 0;
    }
    
    /* UWB_TASK 線程是靜態創建的，不能使用 phOsalUwb_Thread_Delete()
     * 需要直接使用 Zephyr API 來中止和等待線程
     */
    if (uwb_ctrl.task_handle != NULL) {
        struct k_thread* thread = (struct k_thread*)uwb_ctrl.task_handle;
        /* 先嘗試中止線程 */
        k_thread_abort(thread);
        /* 等待線程完全退出 */
        k_thread_join(thread, K_MSEC(300));  /* 增加等待時間到 300ms */
        uwb_ctrl.task_handle = NULL;
    }
    
    /* 清除全局指針 */
    gp_uwbtask_ctrl = NULL;
}

/*******************************************************************************
**
** Function:    GetHalEntryFuncs()
**
** Description: Get the set of HAL entry points.
**
** Returns:     Functions pointers for HAL entry points.
**
*******************************************************************************/
const tHAL_UWB_ENTRY* GetHalEntryFuncs() { return &mHalEntryFuncs; }

#if !(UWBIOT_UWBD_SR040)
/*******************************************************************************
**
** Function:    HalApplyVendorConfigs
**
** Description: Apply the vendor configurations.
**
** Returns:     None.
**
*******************************************************************************/
tUCI_STATUS HalApplyVendorConfigs() {
    tUCI_STATUS status = 0;
    status = (tUCI_STATUS)phNxpUciHal_applyVendorConfig();
    return status;
}
#endif  //!(UWBIOT_UWBD_SR040)

/*******************************************************************************
**
** Function:    HalOpen
**
** Description: Turn on controller, download firmware.
**
** Returns:     None.
**
*******************************************************************************/
void HalOpen(ATTRIBUTE_UNUSED tHAL_UWB_CBACK* p_hal_cback,
             ATTRIBUTE_UNUSED tHAL_UWB_DATA_CBACK* p_data_cback) {
    (void)phNxpUciHal_open(p_hal_cback, p_data_cback);
}

/*******************************************************************************
**
** Function:    HalClose
**
** Description: Turn off controller.
**
** Returns:     None.
**
*******************************************************************************/
void HalClose() { (void)phNxpUciHal_close(); }

/*******************************************************************************
**
** Function:    HalWrite
**
** Description: Write UCI message to the controller.
**
** Returns:     None.
**
*******************************************************************************/
void HalWrite(ATTRIBUTE_UNUSED uint16_t data_len,
              ATTRIBUTE_UNUSED uint8_t* p_data) {
    (void)phNxpUciHal_write(data_len, p_data);
}

/*******************************************************************************
**
** Function:    HalRegisterAppCallback
**
** Description: registers app data call back in tml context.
**
** Returns:     None.
**
*******************************************************************************/
void HalRegisterAppCallback(phHalAppDataCb* recvDataCb) {
    phNxpUciHal_register_appdata_callback(recvDataCb);
}

/*******************************************************************************
**
** Function:    HalIoctl
**
** Description: Calls ioctl to the Uwb driver.
**              If called with a arg value of 0x01 than wired access requested,
**              status of the requst would be updated to p_data.
**              If called with a arg value of 0x00 than wired access will be
**              released, status of the requst would be updated to p_data.
**              If called with a arg value of 0x02 than current p61 state would
*be
**              updated to p_data.
**
** Returns:     -1 or 0.
**
*******************************************************************************/
tUCI_STATUS HalIoctl(ATTRIBUTE_UNUSED long arg,
                     ATTRIBUTE_UNUSED tHAL_UWB_IOCTL* p_data) {
    tUCI_STATUS status = 0;
    status = (tUCI_STATUS)phNxpUciHal_ioctl(arg, p_data);
    return status;
}

/*******************************************************************************
**
** Function:    UwbDeviceInit
**
** Description: Download firmware patch files and apply device configs.
**
** Returns:     None.
**
*******************************************************************************/
tUCI_STATUS UwbDeviceInit(bool recovery) {
    tUCI_STATUS status = 0;
    status = (tUCI_STATUS)phNxpUciHal_uwbDeviceInit(recovery);
    return status;
}

/*******************************************************************************
**
** Function:    isCmdRespPending
**
** Description: This function is get the Response status for the current command
* sent to fw
**
** Returns:     TRUE if response is pending, FALSE otherwise.
**
*******************************************************************************/
bool isCmdRespPending() { return uwb_cb.is_resp_pending; }

/*******************************************************************************
**
** Function:    Hal_setOperationMode
**
** Description: This function is get the Register the Operation Mode as follows
                1: MCTT
                2: CDC
                3: STANDALONE(DEFAULT)
** Returns:     None.
**
*******************************************************************************/
void Hal_setOperationMode(Uwb_operation_mode_t state) {
    /* register the state of the user mode in Hal */
    phNxpUciHal_SetOperatingMode(state);
    // Fira Test mode is enabled
    if (state == kOPERATION_MODE_mctt) {
        /* enable the conformance test mode to avoid uci packet chaining */
        uwb_cb.IsConformaceTestEnabled = TRUE;
    }
    /*register the operating mode in uwb context to use it in Api*/
    uwb_cb.UwbOperatinMode = state;
}
