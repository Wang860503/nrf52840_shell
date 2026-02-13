/*
 * Copyright 2012-2021,2023 NXP.
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

/**
 * \file  phOsalUwb_Thread_posix.c
 * \brief OSAL Implementation.
 */

/** \addtogroup grp_osal_uwb
    @{
 */
/*
************************* Header Files ****************************************
*/
#ifdef UWBIOT_USE_FTR_FILE
#include "uwb_iot_ftr.h"
#else
#include "uwb_iot_ftr_default.h"
#endif

#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include "phNxpLogApis_UwbApi.h"
#include "phOsalUwb.h"
#include "phOsalUwb_Internal.h"

/*
****************************** Macro Definitions ******************************
*/
/** \ingroup grp_osal_uwb
    Value indicates Failure to suspend/resume thread */
#define PH_OSALUWB_THREADPROC_FAILURE (0xFFFFFFFF)
#define UWB_STACK_ALIGNMENT 32

typedef struct {
  struct k_thread thread;      /* Zephyr 執行緒物件 */
  k_thread_stack_t* stack_mem; /* 堆疊記憶體指標 */
} osal_thread_obj_t;

/*
*************************** Function Definitions ******************************
*/

UWBSTATUS phOsalUwb_Thread_Create(void** hThread,
                                  pphOsalUwb_ThreadFunction_t pThreadFunction,
                                  void* pParam) {
  UWBSTATUS wCreateStatus = UWBSTATUS_SUCCESS;
  phOsalUwb_ThreadCreationParams_t* threadparams =
      (phOsalUwb_ThreadCreationParams_t*)pParam;
  osal_thread_obj_t* pThreadObj = NULL;
  size_t stack_size;
  int prio;

  if ((NULL == hThread) || (NULL == pThreadFunction) ||
      (NULL == threadparams)) {
    return PHUWBSTVAL(CID_UWB_OSAL, UWBSTATUS_INVALID_PARAMETER);
  }

  /* 1. 計算 Stack 大小與優先權 */
  /* 注意：檢查 NXP 傳入的 stackdepth 單位，如果是 bytes 則直接用，如果是 words
   * 需 *4 */
  stack_size = threadparams->stackdepth;
  if (stack_size < 1024) {
    stack_size = 1024; /* 給予最小安全值 */
  }

  /* 優先權映射：假設 NXP 傳入的是一般優先權，這裡映射到 Zephyr Preemptible
   * 優先權 */
  prio = K_PRIO_PREEMPT(threadparams->priority);

  /* 2. 動態分配管理結構 */
  pThreadObj = (osal_thread_obj_t*)k_malloc(sizeof(osal_thread_obj_t));
  if (pThreadObj == NULL) {
    LOG_ERR("Failed to allocate thread object");
    return PHUWBSTVAL(CID_UWB_OSAL, PH_OSALUWB_THREAD_CREATION_ERROR);
  }

  /* 3. 動態分配 Stack (必須對齊) */
  size_t alloc_size = ROUND_UP(stack_size, UWB_STACK_ALIGNMENT);

  printk("DEBUG: pThreadObj addr: %p\n", pThreadObj);
  printk("DEBUG: Requesting Stack Size: %d (Original: %d)\n", alloc_size,
         stack_size);
  /* Z_THREAD_STACK_OBJ_ALIGN 是 Zephyr 內部巨集，確保 Stack 對齊硬體要求 */
  pThreadObj->stack_mem = k_aligned_alloc(UWB_STACK_ALIGNMENT, alloc_size);

  if (pThreadObj->stack_mem == NULL) {
    LOG_ERR("Failed to allocate thread stack");
    k_free(pThreadObj);
    return PHUWBSTVAL(CID_UWB_OSAL, PH_OSALUWB_THREAD_CREATION_ERROR);
  }

  /* 4. 建立 Zephyr 執行緒 */
  k_tid_t tid = k_thread_create(
      &pThreadObj->thread,   /* Thread Object */
      pThreadObj->stack_mem, /* Stack Pointer */
      stack_size,            /* Stack Size */
      (k_thread_entry_t)
          pThreadFunction, /* Entry Function (強制轉型符合 k_thread_entry_t) */
      threadparams->pContext, /* p1: Context 參數 */
      NULL,                   /* p2: Unused */
      NULL,                   /* p3: Unused */
      prio,                   /* Priority */
      0,                      /* Options */
      K_NO_WAIT               /* Delay (立即啟動) */
  );

  /* 5. 設定執行緒名稱 (Debug 用) */
  if (threadparams->taskname) {
    k_thread_name_set(tid, threadparams->taskname);
  }

  /* 6. 回傳 Handle (指向我們的 wrapper 結構) */
  *hThread = (void*)pThreadObj;

  LOG_D("Create thread %s. Handle %p",
        threadparams->taskname ? threadparams->taskname : "Unknown",
        pThreadObj);

  return wCreateStatus;
}

UWBSTATUS phOsalUwb_Thread_Delete(UWBOSAL_TASK_HANDLE hThread) {
  UWBSTATUS wDeletionStatus = UWBSTATUS_SUCCESS;
  osal_thread_obj_t* pThreadObj = (osal_thread_obj_t*)hThread;

  if (pThreadObj == NULL) {
    return UWBSTATUS_FAILED;
  }

  LOG_D("Deleting thread %p", hThread);

  /* 1. 中止執行緒 */
  k_thread_abort(&pThreadObj->thread);

  /* 2. 等待它完全停止 (雖然 abort 是同步的，但安全起見) */
  k_thread_join(&pThreadObj->thread, K_MSEC(100));

  /* 3. 釋放資源 */
  if (pThreadObj->stack_mem) {
    k_free(pThreadObj->stack_mem);
  }
  k_free(pThreadObj);

  LOG_DBG("Terminated thread %p", hThread);

  return wDeletionStatus;
}

UWBOSAL_TASK_HANDLE phOsalUwb_GetTaskHandle(void) {
  return (UWBOSAL_TASK_HANDLE)k_current_get();
}

void phOsalUwb_TaskResume(UWBOSAL_TASK_HANDLE xTaskToResume) {
  /* Zephyr: Resume */
  osal_thread_obj_t* pThreadObj = (osal_thread_obj_t*)xTaskToResume;
  if (pThreadObj) {
    k_thread_resume(&pThreadObj->thread);
  }
}

void phOsalUwb_TaskSuspend(UWBOSAL_TASK_HANDLE xTaskToSuspend) {
  /* Zephyr: Suspend */
  osal_thread_obj_t* pThreadObj = (osal_thread_obj_t*)xTaskToSuspend;
  if (pThreadObj) {
    k_thread_suspend(&pThreadObj->thread);
  }
}

void phOsalUwb_TaskStartScheduler(void) {
  /* Zephyr 開機後 Scheduler 自動啟動，無需實作 */
}

void phOsalUwb_Thread_Join(UWBOSAL_TASK_HANDLE hThread) {
  osal_thread_obj_t* pThreadObj = (osal_thread_obj_t*)hThread;
  if (pThreadObj) {
    k_thread_join(&pThreadObj->thread, K_FOREVER);
  }
}

/** @} */
