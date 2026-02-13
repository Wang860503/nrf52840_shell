/*
 * Copyright 2012-2020,2022 NXP.
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
#include "phNxpUciHal_utils.h"

#include <errno.h>
#include <zephyr/sys/sys_heap.h>

#include "phNxpLogApis_UwbApi.h"
#include "phNxpUciHal.h"
#include "phOsalUwb.h"
#include "phUwb_BuildConfig.h"

typedef struct {
  sys_dnode_t node;
  void* pData;
} list_node_wrapper_t;

/*********************** Link list functions **********************************/

/*******************************************************************************
**
** Function         phNxpUciHal_listInit
**
** Description      List initialization
**
** Returns          1, if list initialized, 0 otherwise
**
*******************************************************************************/
int phNxpUciHal_listInit(struct listHead* pList) {
  if (pList == NULL) {
    return 0;
  }
  sys_dlist_init(&pList->dlist);
  k_mutex_init(&pList->mutex);
  return 1;
}

/*******************************************************************************
**
** Function         phNxpUciHal_listDestroy
**
** Description      List destruction
**
** Returns          1, if list destroyed, 0 if failed
**
*******************************************************************************/
int phNxpUciHal_listDestroy(struct listHead* pList) {
  int bListNotEmpty = 1;
  /* 清空所有節點 */
  while (bListNotEmpty) {
    bListNotEmpty = phNxpUciHal_listGetAndRemoveNext(pList, NULL);
  }

  return 1;
}

/*******************************************************************************
**
** Function         phNxpUciHal_listAdd
**
** Description      Add a node to the list
**
** Returns          1, if added, 0 if otherwise
**
*******************************************************************************/
int phNxpUciHal_listAdd(struct listHead* pList, void* pData) {
  list_node_wrapper_t* pWrapper;

  /* 使用 k_malloc 分配節點記憶體 */
  pWrapper = (list_node_wrapper_t*)k_malloc(sizeof(list_node_wrapper_t));
  if (pWrapper == NULL) {
    NXPLOG_UCIX_E("Failed to malloc list node");
    return 0;
  }

  pWrapper->pData = pData;

  k_mutex_lock(&pList->mutex, K_FOREVER);

  /* [優化] O(1) 直接加到尾端，不需要迴圈遍歷 */
  sys_dlist_append(&pList->dlist, &pWrapper->node);

  k_mutex_unlock(&pList->mutex);
  return 1;
}

/*******************************************************************************
**
** Function         phNxpUciHal_listRemove
**
** Description      Remove node from the list
**
** Returns          1, if removed, 0 if otherwise
**
*******************************************************************************/
int phNxpUciHal_listRemove(struct listHead* pList, void* pData) {
  list_node_wrapper_t *pWrapper, *tmp;
  int result = 0;

  k_mutex_lock(&pList->mutex, K_FOREVER);

  if (sys_dlist_is_empty(&pList->dlist)) {
    NXPLOG_UCIX_E("Failed to deallocate (list empty)");
    goto clean_and_return;
  }

  /* [優化] 使用 Zephyr 安全遍歷巨集 */
  SYS_DLIST_FOR_EACH_CONTAINER_SAFE(&pList->dlist, pWrapper, tmp, node) {
    if (pWrapper->pData == pData) {
      /* 找到節點，移除並釋放 */
      sys_dlist_remove(&pWrapper->node);
      k_free(pWrapper);
      result = 1;
      break; /* 找到後即可退出 */
    }
  }

  if (result == 0) {
    NXPLOG_UCIX_E("Failed to deallocate (not found %p)", pData);
  }

clean_and_return:
  k_mutex_unlock(&pList->mutex);
  return result;
}

/*******************************************************************************
**
** Function         phNxpUciHal_listGetAndRemoveNext
**
** Description      Get next node on the list and remove it
**
** Returns          1, if successful, 0 if otherwise
**
*******************************************************************************/
int phNxpUciHal_listGetAndRemoveNext(struct listHead* pList, void** ppData) {
  sys_dnode_t* pNode;
  list_node_wrapper_t* pWrapper;
  int result = 0;

  k_mutex_lock(&pList->mutex, K_FOREVER);

  if (sys_dlist_is_empty(&pList->dlist)) {
    goto clean_and_return;
  }

  /* 取得並移除第一個節點 (Get Head) */
  pNode = sys_dlist_get(&pList->dlist);

  if (pNode != NULL) {
    /* 透過 CONTAINER_OF 巨集還原結構指標 (雖然我們知道它是第一個成員) */
    pWrapper = CONTAINER_OF(pNode, list_node_wrapper_t, node);

    if (ppData != NULL) {
      *ppData = pWrapper->pData;
    }

    /* 釋放節點記憶體 */
    k_free(pWrapper);
    result = 1;
  }

clean_and_return:
  /* 除錯用，非必要可移除 */
  // phNxpUciHal_listDump(pList);
  k_mutex_unlock(&pList->mutex);
  return result;
}

/*******************************************************************************
**
** Function         phNxpUciHal_listDump
**
** Description      Dump list information
**
** Returns          None
**
*******************************************************************************/
void phNxpUciHal_listDump(struct listHead* pList) {
  list_node_wrapper_t* pWrapper;

  NXPLOG_UCIX_D("Node dump:");

  SYS_DLIST_FOR_EACH_CONTAINER(&pList->dlist, pWrapper, node) {
    NXPLOG_UCIX_D("- %p (%p)", pWrapper, pWrapper->pData);
  }
}

/* END Linked list source code */

/****************** Semaphore and mutex helper functions **********************/

static phNxpUciHal_Monitor_t* nxpucihal_monitor = NULL;

/*******************************************************************************
**
** Function         phNxpUciHal_init_monitor
**
** Description      Initialize the semaphore monitor
**
** Returns          Pointer to monitor, otherwise NULL if failed
**
*******************************************************************************/
phNxpUciHal_Monitor_t* phNxpUciHal_init_monitor(void) {
  NXPLOG_UCIX_D("Entering phNxpUciHal_init_monitor");

  UWBSTATUS wStatus;

  if (nxpucihal_monitor == NULL) {
    nxpucihal_monitor = (phNxpUciHal_Monitor_t*)phOsalUwb_GetMemory(
        sizeof(phNxpUciHal_Monitor_t));
  }

  if (nxpucihal_monitor != NULL) {
    phOsalUwb_SetMemory(nxpucihal_monitor, 0x00, sizeof(phNxpUciHal_Monitor_t));
    wStatus = phOsalUwb_CreateBinSem(&nxpucihal_monitor->reentrance_binSem);
    if (wStatus != UWBSTATUS_SUCCESS) {
      NXPLOG_UCIX_E("reentrance_binSem creation returned error: %u", wStatus);
      goto clean_and_return;
    }

    wStatus = phOsalUwb_CreateBinSem(&nxpucihal_monitor->concurrency_binSem);
    if (wStatus != UWBSTATUS_SUCCESS) {
      NXPLOG_UCIX_E("concurrency_binSem creation returned error: %u", wStatus);
      phOsalUwb_DeleteSemaphore(&nxpucihal_monitor->reentrance_binSem);
      goto clean_and_return;
    }

    if (phNxpUciHal_listInit(&nxpucihal_monitor->sem_list) != 1) {
      NXPLOG_UCIX_E("Semaphore List creation failed");
      phOsalUwb_DeleteSemaphore(&nxpucihal_monitor->concurrency_binSem);
      phOsalUwb_DeleteSemaphore(&nxpucihal_monitor->reentrance_binSem);
      goto clean_and_return;
    }
  } else {
    NXPLOG_UCIX_E("nxphal_monitor creation failed");
    goto clean_and_return;
  }

  phOsalUwb_ProduceSemaphore(nxpucihal_monitor->concurrency_binSem);
  phOsalUwb_ProduceSemaphore(nxpucihal_monitor->reentrance_binSem);
  NXPLOG_UCIX_D("Returning with SUCCESS");

  return nxpucihal_monitor;

clean_and_return:
  NXPLOG_UCIX_D("Returning with FAILURE");

  if (nxpucihal_monitor != NULL) {
    phOsalUwb_FreeMemory(nxpucihal_monitor);
    nxpucihal_monitor = NULL;
  }

  return NULL;
}

/*******************************************************************************
**
** Function         phNxpUciHal_cleanup_monitor
**
** Description      Clean up semaphore monitor
**
** Returns          None
**
*******************************************************************************/
void phNxpUciHal_cleanup_monitor(void) {
  if (nxpucihal_monitor != NULL) {
    phOsalUwb_DeleteSemaphore(&nxpucihal_monitor->concurrency_binSem);
    REENTRANCE_UNLOCK();
    (void)phOsalUwb_DeleteSemaphore(&nxpucihal_monitor->reentrance_binSem);
    phNxpUciHal_releaseall_cb_data();
    (void)phNxpUciHal_listDestroy(&nxpucihal_monitor->sem_list);
  }
  phOsalUwb_FreeMemory(nxpucihal_monitor);
  nxpucihal_monitor = NULL;
  return;
}

/*******************************************************************************
**
** Function         phNxpUciHal_get_monitor
**
** Description      Get monitor
**
** Returns          Pointer to monitor
**
*******************************************************************************/
phNxpUciHal_Monitor_t* phNxpUciHal_get_monitor(void) {
  if (nxpucihal_monitor == NULL) {
    NXPLOG_UCIX_E("nxpucihal_monitor is null");
  }
  return nxpucihal_monitor;
}

/* Initialize the callback data */
UWBSTATUS phNxpUciHal_init_cb_data(phNxpUciHal_Sem_t* pCallbackData,
                                   void* pContext) {
  /* Create semaphore */
  if (phOsalUwb_CreateSemaphore(&pCallbackData->sem, 0) != UWBSTATUS_SUCCESS) {
    NXPLOG_UCIX_E("Semaphore creation failed");
    return UWBSTATUS_FAILED;
  }

  /* Set default status value */
  pCallbackData->status = UWBSTATUS_FAILED;

  /* Copy the context */
  pCallbackData->pContext = pContext;

  /* Add to active semaphore list */
  if (phNxpUciHal_listAdd(&phNxpUciHal_get_monitor()->sem_list,
                          pCallbackData) != 1) {
    NXPLOG_UCIX_E("Failed to add the semaphore to the list");
  }

  return UWBSTATUS_SUCCESS;
}

/*******************************************************************************
**
** Function         phNxpUciHal_cleanup_cb_data
**
** Description      Clean up callback data
**
** Returns          None
**
*******************************************************************************/
void phNxpUciHal_cleanup_cb_data(phNxpUciHal_Sem_t* pCallbackData) {
  /* Destroy semaphore */
  UWBSTATUS status;
  /* Destroy semaphore */
  if ((status = phOsalUwb_DeleteSemaphore(&pCallbackData->sem)) !=
      UWBSTATUS_SUCCESS) {
    NXPLOG_UCIX_E(
        "phNxpUciHal_cleanup_cb_data: Failed to destroy semaphore "
        "(status=0x%08x)",
        status);
  }
  /* Remove from active semaphore list */
  if (phNxpUciHal_listRemove(&phNxpUciHal_get_monitor()->sem_list,
                             pCallbackData) != 1) {
    NXPLOG_UCIX_E(
        "phNxpUciHal_cleanup_cb_data: Failed to remove semaphore from the "
        "list");
  }
  return;
}

/*******************************************************************************
**
** Function         phNxpUciHal_releaseall_cb_data
**
** Description      Release all callback data
**
** Returns          None
**
*******************************************************************************/
void phNxpUciHal_releaseall_cb_data(void) {
  phNxpUciHal_Sem_t* pCallbackData;

  while (phNxpUciHal_listGetAndRemoveNext(&phNxpUciHal_get_monitor()->sem_list,
                                          (void**)&pCallbackData)) {
    pCallbackData->status = UWBSTATUS_FAILED;
    phOsalUwb_ProduceSemaphore(pCallbackData->sem);
  }
  return;
}

/* END Semaphore and mutex helper functions */
