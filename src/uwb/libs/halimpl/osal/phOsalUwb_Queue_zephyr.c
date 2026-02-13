#include "phOsalUwb_Queue.h"
#include "phUwbStatus.h"

/*******************************************************************************
**
** Function         phOsalUwb_msgrcv
**
** Description      Gets the oldest message from the queue.
**                  If the queue is empty the function waits (blocks on a mutex)
**                  until a message is posted to the queue with
**                  phOsalUwb_msgsnd.
**
** Parameters       msqid  - message queue handle
**                  msg    - container for the message to be received
**                  waittimeout - max wait time for task in ms
**
** Returns          UWBSTATUS_SUCCESS,  if successful
**                  UWBSTATUS_FAILED, if invalid parameter passed
**
*******************************************************************************/
UWBSTATUS phOsalUwb_msgrcv(intptr_t msqid, phLibUwb_Message_t* msg,
                           unsigned long waittimeout) {
  /* 1. 參數檢查 */
  if ((msqid == 0) || (msg == NULL)) {
    return UWBSTATUS_FAILED;
  }

  struct k_msgq* q = (struct k_msgq*)msqid;
  k_timeout_t wait_time;

  /* 2. 轉換時間參數 (unsigned long -> k_timeout_t) */
  /* 注意：必須與 msgsnd 的邏輯一致 */
  if (waittimeout == 0) {  // 對應 NO_DELAY
    wait_time = K_NO_WAIT;
  } else if (waittimeout == 0xFFFFFFFF) {  // 對應 MAX_DELAY / portMAX_DELAY
    wait_time = K_FOREVER;
  } else {
    wait_time = K_MSEC(waittimeout);
  }

  /* 3. 接收訊息 */
  /* k_msgq_get 會將資料從 Queue 複製到 msg 指向的記憶體 */
  /* 如果 Queue 是空的，它會依照 wait_time 進行等待 */
  int ret = k_msgq_get(q, msg, wait_time);

  if (ret == 0) {
    return UWBSTATUS_SUCCESS;
  } else {
    /* 超時或是發生錯誤 */
    return UWBSTATUS_FAILED;
  }
}

/*******************************************************************************
**
** Function         phOsalUwb_msgsnd
**
** Description      Sends a message to the queue. The message will be added at
**                  the end of the queue as appropriate for FIFO policy
**
** Parameters       msqid  - message queue handle
**                  msg   - message to be sent
**                  waittimeout - ignored
**
** Returns          UWBSTATUS_SUCCESS,  if successful
**                  UWBSTATUS_FAILED, if invalid parameter passed or failed to
* allocate memory
**
*******************************************************************************/
UWBSTATUS phOsalUwb_msgsnd(intptr_t msqid, phLibUwb_Message_t* msg,
                           unsigned long waittimeout) {
  /* 1. 參數檢查 */
  if ((msqid == 0) || (msg == NULL)) {
    return UWBSTATUS_FAILED;
  }

  struct k_msgq* q = (struct k_msgq*)msqid;
  k_timeout_t wait_time;

  /* 2. 轉換時間參數 (NXP -> Zephyr) */
  if (waittimeout == NO_DELAY) {
    wait_time = K_NO_WAIT;
  } else if (waittimeout == MAX_DELAY) {
    wait_time = K_FOREVER;
  } else {
    wait_time = K_MSEC(waittimeout);
  }

  /* 3. 發送訊息 (Zephyr 會自動將 msg 的內容複製一份進 Queue) */
  /* 注意：這裡只複製 phLibUwb_Message_t 結構本身，不會複製 pMsgData
   * 指標指向的內容 */
  /* 所以呼叫此函式前，pMsgData 必須指向有效的 Heap 記憶體 (我們之前的 malloc
   * 修改) */

  int ret = k_msgq_put(q, msg, wait_time);

  if (ret == 0) {
    return UWBSTATUS_SUCCESS;
  } else {
    /* Queue 滿了，或是等待超時 */
    // printk("Error: UWB Msg Queue Full or Timeout\n");
    return UWBSTATUS_FAILED;
  }
}

/*******************************************************************************
**
** Function         phOsalUwb_msgrelease
**
** Description      Releases message queue
**
** Parameters       msqid - message queue handle
**
** Returns          None
**
*******************************************************************************/
void phOsalUwb_msgrelease(intptr_t msqid) {
  struct k_msgq* q = (struct k_msgq*)msqid;

  if (q != NULL) {
    /* 1. 釋放 Ring Buffer */
    /* Zephyr 的 k_msgq 結構中有保存 buffer_start 指標 */
    if (q->buffer_start != NULL) {
      k_free(q->buffer_start);
    }

    /* 2. 釋放 Queue 結構體本身 */
    k_free(q);
  }
}

/*******************************************************************************
**
** Function         phOsalUwb_msgget
**
** Description      Allocates message queue
**
** Parameters       Ignored, included only for Linux queue API compatibility
**
** Returns          (int) value of pQueue if successful
**                  -1, if failed to allocate memory or to init mutex
**
*******************************************************************************/
intptr_t phOsalUwb_msgget(uint32_t queueLength) {
  /* 1. 計算單一訊息的大小 */
  if (queueLength > 200 || queueLength == 0) {
    printk("FATAL: phOsalUwb_msgget called with invalid length: %d\n",
           queueLength);
    return 0;
  }
  size_t msg_size = sizeof(phLibUwb_Message_t);

  /* 2. 分配 Zephyr k_msgq 結構體本身的記憶體 */
  struct k_msgq* pQueue = (struct k_msgq*)k_malloc(sizeof(struct k_msgq));
  if (pQueue == NULL) {
    // LOG_ERR("Failed to allocate queue struct");
    return -1;  // 或回傳 0/NULL，視 NXP 定義而定，通常 -1 代表錯誤
  }

  /* 3. 分配 Queue 實際存放資料的 Ring Buffer 記憶體 */
  /* 大小 = 訊息數量 * 單一訊息大小 */
  char* buffer = (char*)k_malloc(queueLength * msg_size);
  if (buffer == NULL) {
    // LOG_ERR("Failed to allocate queue buffer");
    k_free(pQueue);  // 記得釋放剛剛分配的 struct
    return -1;
  }

  /* 4. 初始化 Zephyr Message Queue */
  /* k_msgq_init(queue, buffer, item_size, max_msgs) */
  k_msgq_init(pQueue, buffer, msg_size, queueLength);

  /* 5. 回傳指標轉型為 intptr_t */
  return (intptr_t)pQueue;
}