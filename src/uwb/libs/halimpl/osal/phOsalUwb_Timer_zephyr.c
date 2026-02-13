/*
 * Copyright 2012-2023 NXP.
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

/*
 * OSAL Implementation for Timers.
 */
#ifndef COMPANION_DEVICE
#include <string.h>
#include <zephyr/kernel.h>

#include "phNxpLogApis_UwbApi.h"
#include "phOsalUwb.h"
#include "phUwbCommon.h"
#include "phUwbTypes.h"

typedef struct {
  struct k_timer timer; /* Zephyr 原生 Timer */
  struct k_work work;   /* 用於切換到 Thread Context 的 Work */
  uint32_t timerId;     /* 儲存 Timer ID */
  bool isAutoReload;    /* 標記是否為週期性 Timer */
  void* parentHandle;   /* 指回 phOsalUwb_TimerHandle_t 以存取 Callback */
} zephyr_timer_wrapper_t;

/*
 * 2 Timers are used. One each by gki and UciHal.
 */
#define PH_UWB_MAX_TIMER (2U)

static phOsalUwb_TimerHandle_t apTimerInfo[PH_UWB_MAX_TIMER];

/*
 * Defines the base address for generating timerid.
 */
#define PH_UWB_TIMER_BASE_ADDRESS (100U)

/*
 *  Defines the value for invalid timerid returned during timeSetEvent
 */
#define PH_UWB_TIMER_ID_ZERO (0x00)

/*
 * Invalid timer ID type. This ID used indicate timer creation is failed */
#define PH_UWB_TIMER_ID_INVALID (0xFFFF)

/* Forward declarations */
static void phOsalUwb_Zephyr_Timer_Expiry(struct k_timer* timer);
static void phOsalUwb_Zephyr_Work_Handler(struct k_work* work);
uint32_t phUtilUwb_CheckForAvailableTimer(void);
UWBSTATUS phOsalUwb_CheckTimerPresence(void* pObjectHandle);

/*
 *************************** Function Definitions ******************************
 */

/** \addtogroup grp_osal_timer
 *
 * @{
 */
/*******************************************************************************
**
** Function         phOsalUwb_Timer_Create
**
** Description      Creates a timer which shall call back the specified function
*when the timer expires
**                  Fails if OSAL module is not initialized or timers are
*already occupied
**
** Parameters       bAutoReload
**                  If bAutoReload is set to TRUE then the timer will
**                  expire repeatedly with a frequency set by the
*xTimerPeriodInTicks parameter.
**                  Timer restarts automatically.
**                  If bAutoReload is set to FALSE then the timer will be a
*one-shot timer and
**                  enter the dormant state after it expires.
**
** Returns          TimerId
**                  TimerId value of PH_OSALUWB_TIMER_ID_INVALID indicates that
*timer is not created                -
**
*******************************************************************************/
uint32_t phOsalUwb_Timer_Create(uint8_t bAutoReload) {
  uint32_t dwTimerId;
  phOsalUwb_TimerHandle_t* pTimerHandle;
  zephyr_timer_wrapper_t* pWrapper;

  dwTimerId = phUtilUwb_CheckForAvailableTimer();

  if ((PH_UWB_TIMER_ID_ZERO != dwTimerId) && (dwTimerId <= PH_UWB_MAX_TIMER)) {
    pTimerHandle = (phOsalUwb_TimerHandle_t*)&apTimerInfo[dwTimerId - 1];

    dwTimerId += PH_UWB_TIMER_BASE_ADDRESS;

    /* [Zephyr Porting] 分配 Wrapper 記憶體 */
    pWrapper =
        (zephyr_timer_wrapper_t*)k_malloc(sizeof(zephyr_timer_wrapper_t));

    if (pWrapper != NULL) {
      memset(pWrapper, 0, sizeof(zephyr_timer_wrapper_t));

      /* 設定 Wrapper 參數 */
      pWrapper->timerId = dwTimerId;
      pWrapper->isAutoReload = (bool)bAutoReload;
      pWrapper->parentHandle = (void*)pTimerHandle;

      /* 初始化 Zephyr Timer (ISR Context) */
      k_timer_init(&pWrapper->timer, phOsalUwb_Zephyr_Timer_Expiry, NULL);

      /* 初始化 Zephyr Work (Thread Context) */
      k_work_init(&pWrapper->work, phOsalUwb_Zephyr_Work_Handler);

      /* 將 Wrapper 掛載到 hTimerHandle */
      pTimerHandle->hTimerHandle = (void*)pWrapper;

      /* 初始化狀態 */
      pTimerHandle->eState = eTimerIdle;
      pTimerHandle->TimerId = dwTimerId;
    } else {
      NXPLOG_UCIX_E("Failed to allocate memory for Zephyr timer wrapper");
      dwTimerId = PH_UWB_TIMER_ID_INVALID;
    }
  } else {
    dwTimerId = PH_UWB_TIMER_ID_INVALID;
  }

  return dwTimerId;
}

/*******************************************************************************
**
** Function         phOsalUwb_Timer_Start
**
** Description      Starts the requested, already created, timer
**                  If the timer is already running, timer stops and restarts
*with the new timeout value
**                  and new callback function in case any ??????
**                  Creates a timer which shall call back the specified function
*when the timer expires
**
** Parameters       dwTimerId             - valid timer ID obtained during timer
*creation
**                  dwRegTimeCnt          - requested timeout in milliseconds
**                  pApplication_callback - application callback interface to be
*called when timer expires
**                  pContext              - caller context, to be passed to the
*application callback function
**
** Returns          UWB status:
**                  UWBSTATUS_SUCCESS            - the operation was successful
**                  UWBSTATUS_NOT_INITIALISED    - OSAL Module is not
*initialized
**                  UWBSTATUS_INVALID_PARAMETER  - invalid parameter passed to
*the function
**                  PH_OSALUWB_TIMER_START_ERROR - timer could not be created
*due to system error
**
*******************************************************************************/
UWBSTATUS phOsalUwb_Timer_Start(uint32_t dwTimerId, uint32_t dwRegTimeCnt,
                                pphOsalUwb_TimerCallbck_t pApplication_callback,
                                void* pContext) {
  UWBSTATUS wStartStatus = UWBSTATUS_SUCCESS;
  uint32_t dwIndex;
  phOsalUwb_TimerHandle_t* pTimerHandle;
  zephyr_timer_wrapper_t* pWrapper;

  dwIndex = dwTimerId - PH_UWB_TIMER_BASE_ADDRESS - 0x01;
  pTimerHandle = (phOsalUwb_TimerHandle_t*)&apTimerInfo[dwIndex];

  if ((dwIndex < PH_UWB_MAX_TIMER) && (0x00 != pTimerHandle->TimerId) &&
      (NULL != pApplication_callback) && (NULL != pTimerHandle->hTimerHandle)) {
    /* 從 void* 取回我們的 Wrapper */
    pWrapper = (zephyr_timer_wrapper_t*)pTimerHandle->hTimerHandle;

    /* 更新 Callback 資訊 */
    pTimerHandle->Application_callback = pApplication_callback;
    pTimerHandle->pContext = pContext;
    pTimerHandle->eState = eTimerRunning;

    /* [Zephyr Porting] 啟動 Timer
     * k_timer_start(timer, duration, period)
     * - duration: 第一次到期的時間
     * - period: 之後重複的週期 (如果是 One-Shot 則設為 K_NO_WAIT)
     */
    k_timeout_t duration = K_MSEC(dwRegTimeCnt);
    k_timeout_t period =
        (pWrapper->isAutoReload) ? K_MSEC(dwRegTimeCnt) : K_NO_WAIT;

    k_timer_start(&pWrapper->timer, duration, period);
  } else {
    wStartStatus = PHUWBSTVAL(CID_UWB_OSAL, UWBSTATUS_INVALID_PARAMETER);
  }

  return wStartStatus;
}

/*******************************************************************************
**
** Function         phOsalUwb_Timer_Stop
**
** Description      Stops already started timer
**                  Allows to stop running timer. In case timer is stopped,
*timer callback
**                  will not be notified any more
**
** Parameters       dwTimerId             - valid timer ID obtained during timer
*creation
**
** Returns          UWB status:
**                  UWBSTATUS_SUCCESS            - the operation was successful
**                  UWBSTATUS_NOT_INITIALISED    - OSAL Module is not
*initialized
**                  UWBSTATUS_INVALID_PARAMETER  - invalid parameter passed to
*the function
**                  PH_OSALUWB_TIMER_STOP_ERROR  - timer could not be stopped
*due to system error
**
*******************************************************************************/
UWBSTATUS phOsalUwb_Timer_Stop(uint32_t dwTimerId) {
  UWBSTATUS wStopStatus = UWBSTATUS_SUCCESS;
  uint32_t dwIndex;
  phOsalUwb_TimerHandle_t* pTimerHandle;
  zephyr_timer_wrapper_t* pWrapper;

  dwIndex = dwTimerId - PH_UWB_TIMER_BASE_ADDRESS - 0x01;
  pTimerHandle = (phOsalUwb_TimerHandle_t*)&apTimerInfo[dwIndex];

  if ((dwIndex < PH_UWB_MAX_TIMER) && (0x00 != pTimerHandle->TimerId) &&
      (pTimerHandle->eState != eTimerIdle)) {
    pWrapper = (zephyr_timer_wrapper_t*)pTimerHandle->hTimerHandle;

    /* 停止 Timer */
    k_timer_stop(&pWrapper->timer);

    /* [重要] 取消尚未執行的 Work，避免 Race Condition (例如剛到期但還沒執行
     * Callback) */
    k_work_cancel(&pWrapper->work);

    pTimerHandle->eState = eTimerStopped;
  } else {
    wStopStatus = PHUWBSTVAL(CID_UWB_OSAL, UWBSTATUS_INVALID_PARAMETER);
  }

  return wStopStatus;
}
/*******************************************************************************
**
** Function         phOsalUwb_Timer_Delete
**
** Description      Deletes previously created timer
**                  Allows to delete previously created timer. In case timer is
*running,
**                  it is first stopped and then deleted
**
** Parameters       dwTimerId             - valid timer ID obtained during timer
*creation
**
** Returns          UWB status:
**                  UWBSTATUS_SUCCESS             - the operation was successful
**                  UWBSTATUS_NOT_INITIALISED     - OSAL Module is not
*initialized
**                  UWBSTATUS_INVALID_PARAMETER   - invalid parameter passed to
*the function
**                  PH_OSALUWB_TIMER_DELETE_ERROR - timer could not be stopped
*due to system error
**
*******************************************************************************/
UWBSTATUS phOsalUwb_Timer_Delete(uint32_t dwTimerId) {
  UWBSTATUS wDeleteStatus = UWBSTATUS_SUCCESS;
  uint32_t dwIndex;
  phOsalUwb_TimerHandle_t* pTimerHandle;
  zephyr_timer_wrapper_t* pWrapper;

  dwIndex = dwTimerId - PH_UWB_TIMER_BASE_ADDRESS - 0x01;
  pTimerHandle = (phOsalUwb_TimerHandle_t*)&apTimerInfo[dwIndex];

  if ((dwIndex < PH_UWB_MAX_TIMER) && (0x00 != pTimerHandle->TimerId) &&
      (UWBSTATUS_SUCCESS == phOsalUwb_CheckTimerPresence(pTimerHandle))) {
    pWrapper = (zephyr_timer_wrapper_t*)pTimerHandle->hTimerHandle;

    if (pWrapper != NULL) {
      /* 停止並清理 */
      k_timer_stop(&pWrapper->timer);
      k_work_cancel(&pWrapper->work);

      /* 釋放 Wrapper */
      k_free(pWrapper);
      pTimerHandle->hTimerHandle = NULL;
    }

    /* 清空管理結構 */
    memset(pTimerHandle, 0, sizeof(phOsalUwb_TimerHandle_t));
  } else {
    wDeleteStatus = PHUWBSTVAL(CID_UWB_OSAL, UWBSTATUS_INVALID_PARAMETER);
  }
  return wDeleteStatus;
}

/* *
 * -----------------------------------------------------------------------------
 * Zephyr Internal Handlers (Bottom Half)
 * -----------------------------------------------------------------------------
 */
/* * 1. ISR Stage: Timer 到期 (Interrupt Context)
 * 絕對不能在此處呼叫 User Callback，因為它可能會 Sleep (Mutex)。
 * 我們的動作：只送出一個 Work Request。
 */
static void phOsalUwb_Zephyr_Timer_Expiry(struct k_timer* timer) {
  zephyr_timer_wrapper_t* pWrapper =
      CONTAINER_OF(timer, zephyr_timer_wrapper_t, timer);
  k_work_submit(&pWrapper->work);
}

/* * 2. Thread Stage: Work Handler (Thread Context)
 * 這是 System Workqueue 執行緒，可以安全執行 User Callback。
 */
static void phOsalUwb_Zephyr_Work_Handler(struct k_work* work) {
  zephyr_timer_wrapper_t* pWrapper =
      CONTAINER_OF(work, zephyr_timer_wrapper_t, work);
  phOsalUwb_TimerHandle_t* pTimerHandle =
      (phOsalUwb_TimerHandle_t*)pWrapper->parentHandle;

  if (pTimerHandle && pTimerHandle->Application_callback) {
    /* 如果是 One-Shot，更新狀態 */
    if (!pWrapper->isAutoReload) {
      pTimerHandle->eState = eTimerStopped;
    }

    /* 呼叫使用者的 Callback */
    /* * 注意：原始 NXP 設計這裡傳入的是 TimerId 和 Context。
     * 我們從 Wrapper 取出 TimerId，從 pTimerHandle 取出 Context。
     */
    pTimerHandle->Application_callback(pWrapper->timerId,
                                       pTimerHandle->pContext);
  }
}

/*******************************************************************************
**
** Function         phUtilUwb_CheckForAvailableTimer
**
** Description      Find an available timer id
**
** Parameters       void
**
** Returns          Available timer id
**
*******************************************************************************/
uint32_t phUtilUwb_CheckForAvailableTimer(void) {
  uint32_t dwIndex = 0x00;
  uint32_t dwRetval = 0x00;

  for (dwIndex = 0x00; ((dwIndex < PH_UWB_MAX_TIMER) && (0x00 == dwRetval));
       dwIndex++) {
    if (!(apTimerInfo[dwIndex].TimerId)) {
      dwRetval = (dwIndex + 0x01);
    }
  }
  return (dwRetval);
}

/*******************************************************************************
**
** Function         phOsalUwb_CheckTimerPresence
**
** Description      Checks the requested timer is present or not
**
** Parameters       pObjectHandle - timer context
**
** Returns          UWBSTATUS_SUCCESS if found
**                  Other value if not found
**
*******************************************************************************/
UWBSTATUS phOsalUwb_CheckTimerPresence(void* pObjectHandle) {
  uint32_t dwIndex;
  UWBSTATUS wRegisterStatus = UWBSTATUS_INVALID_PARAMETER;

  for (dwIndex = 0x00;
       ((dwIndex < PH_UWB_MAX_TIMER) && (wRegisterStatus != UWBSTATUS_SUCCESS));
       dwIndex++) {
    if (((&apTimerInfo[dwIndex]) == (phOsalUwb_TimerHandle_t*)pObjectHandle) &&
        (apTimerInfo[dwIndex].TimerId)) {
      wRegisterStatus = UWBSTATUS_SUCCESS;
    }
  }
  return wRegisterStatus;
}

#endif

/** @} */
