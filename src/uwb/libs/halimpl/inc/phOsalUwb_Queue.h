#ifndef PHOSALUWB_QUEUE_H
#define PHOSALUWB_QUEUE_H

#include <zephyr/kernel.h>

#include "phUwbTypes.h"

#ifndef NO_DELAY
#define NO_DELAY 0
#endif

#ifndef MAX_DELAY
#define MAX_DELAY 0xFFFFFFFF
#endif

#define configTML_QUEUE_LENGTH 50

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
/**
 * Gets the oldest message from the queue.
 * If the queue is empty the function waits (blocks on a mutex)
 * until a message is posted to the queue with phOsalUwb_msgsnd.
 *
 * \param[in] msqid   message queue handle
 * \param[in] msg     container for the message to be received
 * \param[in] waittimeout  max wait time for task in ms
 *
 * \retval #UWBSTATUS_SUCCESS if successful
 * \retval #UWBSTATUS_FAILED  if invalid parameter passed
 *
 */
UWBSTATUS phOsalUwb_msgrcv(intptr_t msqid, phLibUwb_Message_t* msg,
                           unsigned long waittimeout);

/**
 * Sends a message to the queue. The message will be added at
 * the end of the queue as appropriate for FIFO policy
 *
 * \param[in] msqid  message queue handle
 * \param[in] msg    message to be sent
 * \param[in] waittimeout  timeout in ms
 *
 * \retval #UWBSTATUS_SUCCESS  if successful
 * \retval #UWBSTATUS_FAILED   if invalid parameter passed or failed to allocate
 * memory
 *
 */
UWBSTATUS phOsalUwb_msgsnd(intptr_t msqid, phLibUwb_Message_t* msg,
                           unsigned long waittimeout);

/**
 * Releases message queue
 *
 * \param[in] msqid message queue handle
 *
 * \retval          None
 *
 */
void phOsalUwb_msgrelease(intptr_t msqid);

/**
 * Allocates message queue
 *
 * \param[in] queueLength Length of the queue ignored for linux
 *
 * \retval          (int) value of pQueue if successful
 *                  -1, if failed to allocate memory or to init mutex
 *
 */
intptr_t phOsalUwb_msgget(uint32_t queueLength);

#endif