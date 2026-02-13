#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/kernel.h>

#include "phNxpLogApis_UwbApi.h"
#include "phOsalUwb_Internal.h"

/*
*************************** Function Definitions ******************************
*/

/*!
 * \brief Allocates memory.
 *        This function attempts to allocate \a size bytes on the heap and
 *        returns a pointer to the allocated block.
 *
 * \param size size of the memory block to be allocated on the heap.
 *
 * \return pointer to allocated memory block or NULL in case of error.
 */
void* phOsalUwb_GetMemory(uint32_t dwSize) {
  // printk("%s: size = %d\n", __FUNCTION__, dwSize);
  return k_malloc(dwSize);
}

/*!
 * \brief Frees allocated memory block.
 *        This function deallocates memory region pointed to by \a pMem.
 *
 * \param pMem pointer to memory block to be freed.
 */
void phOsalUwb_FreeMemory(void* pMem) {
  /* Check whether a null pointer is passed */
  if (NULL != pMem) {
    k_free(pMem);
  }
}

void phOsalUwb_SetMemory(void* pMem, uint8_t bVal, uint32_t dwSize) {
  memset(pMem, bVal, dwSize);
}

void phOsalUwb_MemCopy(void* pDest, const void* pSrc, uint32_t dwSize) {
  memcpy(pDest, pSrc, dwSize);
}

int32_t phOsalUwb_MemCompare(const void* pDest, const void* pSrc,
                             uint32_t dwSize) {
  return memcmp(pDest, pSrc, dwSize);
}

void phOsalUwb_Delay(uint32_t dwDelay) {
  if (k_is_in_isr()) {
    /* 如果在中斷中，使用忙碌等待 (單位是微秒，所以 * 1000) */
    k_busy_wait(dwDelay * 1000);
  } else {
    /* 如果在 Thread 中，使用睡眠 (讓出 CPU) */
    k_msleep(dwDelay);
  }
}

/*******************************************************************************
**
** Function         phOsalUwb_CreateSemaphore
**
** Description      Creates a semaphore
**
*******************************************************************************/
UWBSTATUS phOsalUwb_CreateSemaphore(void** hSemaphore, uint8_t bInitialValue) {
  UWBSTATUS wCreateStatus = UWBSTATUS_SUCCESS;
  phOsalUwb_sOsalSemaphore_t* pSemaphoreHandle = NULL;

  /* Check input parameters */
  if (NULL == hSemaphore) {
    wCreateStatus = PHUWBSTVAL(CID_UWB_OSAL, UWBSTATUS_INVALID_PARAMETER);
  } else {
    pSemaphoreHandle = (phOsalUwb_sOsalSemaphore_t*)phOsalUwb_GetMemory(
        sizeof(phOsalUwb_sOsalSemaphore_t));

    if (pSemaphoreHandle == NULL) {
      printk("Error: phOsalUwb_GetMemory failed!\n");
      wCreateStatus =
          PHUWBSTVAL(CID_UWB_OSAL, PH_OSALUWB_SEMAPHORE_CREATION_ERROR);
      return wCreateStatus;
    }

    /* * 2. 初始化 Zephyr Semaphore
     * k_sem_init(&sem, initial_count, limit)
     * NXP 原始碼 xSemaphoreCreateCounting(1, bInitialValue) 暗示這是 Binary
     * Semaphore (Limit=1)
     */
    int ret = k_sem_init(&pSemaphoreHandle->ObjectHandle,
                         (unsigned int)bInitialValue, 1);

    if (ret != 0) {
      wCreateStatus =
          PHUWBSTVAL(CID_UWB_OSAL, PH_OSALUWB_SEMAPHORE_CREATION_ERROR);
      k_free(pSemaphoreHandle);
      *hSemaphore = NULL;
    } else {
      /* Return the handle (pointer to our wrapper struct) */
      *hSemaphore = (void*)pSemaphoreHandle;
    }
  }
  return wCreateStatus;
}

/*******************************************************************************
**
** Function         phOsalUwb_CreateBinSem
**
** Description      Creates a Binary Semaphore
**
*******************************************************************************/
UWBSTATUS phOsalUwb_CreateBinSem(void** hBinSem) {
  UWBSTATUS wCreateStatus = UWBSTATUS_SUCCESS;
  phOsalUwb_sOsalSemaphore_t* pBinSemHandle = NULL;

  /* Check input parameters */
  if (NULL != hBinSem) {
    /* 1. Allocate memory for the wrapper struct */
    pBinSemHandle = (phOsalUwb_sOsalSemaphore_t*)k_malloc(
        sizeof(phOsalUwb_sOsalSemaphore_t));

    if (pBinSemHandle == NULL) {
      wCreateStatus =
          PHUWBSTVAL(CID_UWB_OSAL, PH_OSALUWB_SEMAPHORE_CREATION_ERROR);
      return wCreateStatus;
    }

    /* * 2. Initialize Zephyr Semaphore
     * k_sem_init(&sem, initial_count, limit)
     * * [Porting Note]
     * FreeRTOS xSemaphoreCreateBinary() creates a semaphore that is "Empty" (0)
     * initially. Therefore, we set initial_count = 0, limit = 1.
     */
    int ret = k_sem_init(&pBinSemHandle->ObjectHandle, 0, 1);

    if (ret != 0) {
      /* Initialization failed */
      wCreateStatus =
          PHUWBSTVAL(CID_UWB_OSAL, PH_OSALUWB_SEMAPHORE_CREATION_ERROR);
      k_free(pBinSemHandle);
      *hBinSem = NULL;
    } else {
      /* Success */
      *hBinSem = (void*)pBinSemHandle;
    }
  } else {
    wCreateStatus = PHUWBSTVAL(CID_UWB_OSAL, UWBSTATUS_INVALID_PARAMETER);
  }

  return wCreateStatus;
}

/*******************************************************************************
**
** Function         phOsalUwb_ProduceSemaphore
**
** Description      Gives (Signals) the semaphore
**
*******************************************************************************/
UWBSTATUS phOsalUwb_ProduceSemaphore(void* hSemaphore) {
  UWBSTATUS wReleaseStatus = UWBSTATUS_SUCCESS;
  phOsalUwb_sOsalSemaphore_t* pSemaphoreHandle =
      (phOsalUwb_sOsalSemaphore_t*)hSemaphore;

  if (pSemaphoreHandle != NULL) {
    /* * Zephyr Porting Note:
     * k_sem_give 是 ISR-safe 的，不需要區分 Context。
     * 它沒有回傳值 (void)，如果計數已滿 (1)，則保持不變，不會報錯。
     * 這符合 "Give" 的語意。
     */
    k_sem_give(&pSemaphoreHandle->ObjectHandle);
  } else {
    wReleaseStatus = PHUWBSTVAL(CID_UWB_OSAL, UWBSTATUS_INVALID_PARAMETER);
  }

  return wReleaseStatus;
}

/*******************************************************************************
**
** Function         phOsalUwb_ConsumeSemaphore_WithTimeout
**
** Description      Takes (Waits for) the semaphore
**
*******************************************************************************/
UWBSTATUS phOsalUwb_ConsumeSemaphore_WithTimeout(void* hSemaphore,
                                                 uint32_t delay) {
  UWBSTATUS wConsumeStatus = UWBSTATUS_SUCCESS;
  phOsalUwb_sOsalSemaphore_t* pSemaphoreHandle =
      (phOsalUwb_sOsalSemaphore_t*)hSemaphore;
  k_timeout_t timeout;
  int ret;

  if (pSemaphoreHandle != NULL) {
    /* * Zephyr Porting Note:
     * 1. 設定超時時間
     */
    timeout = K_MSEC(delay);

    /* * 2. ISR Context 檢查
     * 如果在中斷中呼叫，Zephyr 不允許睡眠，必須強制使用 K_NO_WAIT。
     */
    if (k_is_in_isr()) {
      timeout = K_NO_WAIT;
    }

    /* * 3. Take Semaphore
     * k_sem_take 回傳 0 表示成功，非 0 表示超時或 Busy
     */
    ret = k_sem_take(&pSemaphoreHandle->ObjectHandle, timeout);

    if (ret != 0) {
      /* * ret == -EBUSY (K_NO_WAIT failed)
       * ret == -EAGAIN (Timeout)
       */
      wConsumeStatus =
          PHUWBSTVAL(CID_UWB_OSAL, PH_OSALUWB_SEMAPHORE_CONSUME_ERROR);
    }
  } else {
    wConsumeStatus = PHUWBSTVAL(CID_UWB_OSAL, UWBSTATUS_INVALID_PARAMETER);
  }

  return wConsumeStatus;
}

/*******************************************************************************
**
** Function         phOsalUwb_DeleteSemaphore
**
** Description      Deletes the semaphore
**
*******************************************************************************/
UWBSTATUS phOsalUwb_DeleteSemaphore(void** hSemaphore) {
  UWBSTATUS wDeletionStatus = UWBSTATUS_SUCCESS;

  /* 檢查 hSemaphore 指標本身是否有效 */
  if (hSemaphore != NULL && *hSemaphore != NULL) {
    phOsalUwb_sOsalSemaphore_t* pSemaphoreHandle =
        (phOsalUwb_sOsalSemaphore_t*)*hSemaphore;

    /*
     * Zephyr Porting Note:
     * Zephyr 沒有 k_sem_delete()。
     * 對於動態分配的物件，直接釋放記憶體即可。
     * (為了安全，可選用 k_sem_reset 重置狀態，但 k_free 已足夠)
     */

    k_free(pSemaphoreHandle);
    *hSemaphore = NULL;
  } else {
    wDeletionStatus = PHUWBSTVAL(CID_UWB_OSAL, UWBSTATUS_INVALID_PARAMETER);
  }

  return wDeletionStatus;
}

/*******************************************************************************
**
** Function         phOsalUwb_CreateMutex
**
** Description      Creates a mutex
**
*******************************************************************************/
UWBSTATUS phOsalUwb_CreateMutex(void** hMutex) {
  UWBSTATUS wCreateStatus = UWBSTATUS_SUCCESS;
  phOsalUwb_sOsalMutex_t* pMutexHandle = NULL;

  if (NULL != hMutex) {
    /* 1. 動態分配 Wrapper 記憶體 */
    pMutexHandle =
        (phOsalUwb_sOsalMutex_t*)k_malloc(sizeof(phOsalUwb_sOsalMutex_t));

    if (pMutexHandle == NULL) {
      wCreateStatus = PHUWBSTVAL(CID_UWB_OSAL, PH_OSALUWB_MUTEX_CREATION_ERROR);
      return wCreateStatus;
    }

    /* 2. 初始化 Zephyr Mutex  */
    int ret = k_mutex_init(&pMutexHandle->ObjectHandle);

    if (ret != 0) {
      wCreateStatus = PHUWBSTVAL(CID_UWB_OSAL, PH_OSALUWB_MUTEX_CREATION_ERROR);
      k_free(pMutexHandle);
      *hMutex = NULL;
    } else {
      /* 3. 回傳 Handle (指標轉型) */
      *hMutex = (void*)pMutexHandle;
    }
  } else {
    wCreateStatus = PHUWBSTVAL(CID_UWB_OSAL, UWBSTATUS_INVALID_PARAMETER);
  }

  return wCreateStatus;
}

/*******************************************************************************
**
** Function         phOsalUwb_LockMutex
**
** Description      Locks the mutex
**
*******************************************************************************/
UWBSTATUS phOsalUwb_LockMutex(void* hMutex) {
  UWBSTATUS wLockStatus = UWBSTATUS_SUCCESS;
  phOsalUwb_sOsalMutex_t* pMutexHandle = (phOsalUwb_sOsalMutex_t*)hMutex;

  if (pMutexHandle != NULL) {
    /* * Zephyr Porting Note:
     * Zephyr Kernel 禁止在 ISR Context 中鎖定 Mutex。
     * 如果程式執行到這裡且 k_is_in_isr() 為真，代表上層邏輯設計有誤，
     * 或者應該改用 k_spinlock / k_sem。
     */
    if (k_is_in_isr()) {
      /* 在 ISR 中無法 Lock Mutex，回傳錯誤 */
      return PHUWBSTVAL(CID_UWB_OSAL, PH_OSALUWB_MUTEX_LOCK_ERROR);
    }

    /* * k_mutex_lock
     * K_FOREVER: 等同於 FreeRTOS 的 MAX_DELAY (無限等待)
     * Zephyr Mutex 支援 Reentrancy (同一個 Thread 可重複 Lock)，這符合 NXP
     * 需求。
     */
    int ret = k_mutex_lock(&pMutexHandle->ObjectHandle, K_FOREVER);

    if (ret != 0) {
      wLockStatus = PHUWBSTVAL(CID_UWB_OSAL, PH_OSALUWB_MUTEX_LOCK_ERROR);
    }
  } else {
    wLockStatus = PHUWBSTVAL(CID_UWB_OSAL, UWBSTATUS_INVALID_PARAMETER);
  }

  return wLockStatus;
}

/*******************************************************************************
**
** Function         phOsalUwb_UnlockMutex
**
** Description      Unlocks the mutex
**
*******************************************************************************/
UWBSTATUS phOsalUwb_UnlockMutex(void* hMutex) {
  UWBSTATUS wUnlockStatus = UWBSTATUS_SUCCESS;
  phOsalUwb_sOsalMutex_t* pMutexHandle = (phOsalUwb_sOsalMutex_t*)hMutex;

  if (pMutexHandle != NULL) {
    if (k_is_in_isr()) {
      /* Zephyr 禁止在 ISR 解鎖 Mutex */
      return PHUWBSTVAL(CID_UWB_OSAL, PH_OSALUWB_MUTEX_UNLOCK_ERROR);
    }

    /* * k_mutex_unlock
     * 釋放 Mutex 所有權。如果有較高優先權的 Thread 在等待，會立即觸發 Context
     * Switch。
     */
    int ret = k_mutex_unlock(&pMutexHandle->ObjectHandle);

    if (ret != 0) {
      /* 例如：解鎖了不屬於自己的 Mutex，或重複解鎖 */
      wUnlockStatus = PHUWBSTVAL(CID_UWB_OSAL, PH_OSALUWB_MUTEX_UNLOCK_ERROR);
    }
  } else {
    wUnlockStatus = PHUWBSTVAL(CID_UWB_OSAL, UWBSTATUS_INVALID_PARAMETER);
  }

  return wUnlockStatus;
}

/*******************************************************************************
**
** Function         phOsalUwb_DeleteMutex
**
** Description      Deletes the mutex
**
*******************************************************************************/
UWBSTATUS phOsalUwb_DeleteMutex(void** hMutex) {
  UWBSTATUS wDeletionStatus = UWBSTATUS_SUCCESS;

  if (hMutex != NULL && *hMutex != NULL) {
    phOsalUwb_sOsalMutex_t* pMutexHandle = (phOsalUwb_sOsalMutex_t*)*hMutex;

    /* * Zephyr Porting Note:
     * k_mutex 不需要像 FreeRTOS 那樣呼叫 explicit delete 函式，
     * 只要它沒被鎖定，直接釋放記憶體即可。
     */

    k_free(pMutexHandle);
    *hMutex = NULL;
  } else {
    wDeletionStatus = PHUWBSTVAL(CID_UWB_OSAL, UWBSTATUS_INVALID_PARAMETER);
  }

  return wDeletionStatus;
}