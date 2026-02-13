/*!
 * *********************************************************************************
 * \defgroup Private profile Service
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
 * This file is the interface file for the QPP Service
 *
 * SPDX-License-Identifier: BSD-3-Clause
 **********************************************************************************
 * */

#ifndef _PRIVATE_PROFILE_INTERFACE_H_
#define _PRIVATE_PROFILE_INTERFACE_H_

/************************************************************************************
*************************************************************************************
* Include
*************************************************************************************
************************************************************************************/
#include <bluetooth/services/nus.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/types.h>
/************************************************************************************
*************************************************************************************
* Public constants & macros
*************************************************************************************
************************************************************************************/
typedef int bleResult_t;
#define gBleSuccess_c 0
#define gBleUnknown_c -1
typedef uint8_t deviceId_t;
/************************************************************************************
*************************************************************************************
* Public type definitions
*************************************************************************************
************************************************************************************/

/*! QPP Service - Configuration */
typedef struct qppsConfig_tag {
  uint16_t serviceHandle;
} qppsConfig_t;

typedef struct tmcConfig_tag {
  uint16_t hService;
  uint16_t hTxData;
  uint16_t hTxCccd;
  uint16_t hRxData;
} qppConfig_t;

/************************************************************************************
*************************************************************************************
* Public memory declarations
*************************************************************************************
************************************************************************************/
extern uint8_t nearby_accessory_data[48];
/************************************************************************************
*************************************************************************************
* Public prototypes
*************************************************************************************
************************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

/*!**********************************************************************************
 * \brief        Starts QPP Service functionality
 *
 * \param[in]    pServiceConfig  Pointer to structure that contains server
 *                               configuration information.
 *
 * \return       gBleSuccess_c or error.
 ************************************************************************************/
static inline bleResult_t Qpp_Start(qppsConfig_t* pServiceConfig) {
  ARG_UNUSED(pServiceConfig);
  return gBleSuccess_c;
}

/*!**********************************************************************************
 * \brief        Stops QPP Service functionality
 *
 * \param[in]    pServiceConfig  Pointer to structure that contains server
 *                               configuration information.
 *
 * \return       gBleSuccess_c or error.
 ************************************************************************************/
static inline bleResult_t Qpp_Stop(qppsConfig_t* pServiceConfig) {
  ARG_UNUSED(pServiceConfig);
  return gBleSuccess_c;
}

/*!**********************************************************************************
 * \brief        Subscribes a GATT client to the QPP service
 *
 * \param[in]    pClient  Client Id in Device DB.
 *
 * \return       gBleSuccess_c or error.
 ************************************************************************************/
bleResult_t Qpp_Subscribe(struct bt_conn* conn);

/*!**********************************************************************************
 * \brief        Unsubscribes a GATT client from the QPP service
 *
 * \return       gBleSuccess_c or error.
 ************************************************************************************/
bleResult_t Qpp_Unsubscribe(void);

/*!**********************************************************************************
 * \brief        Qpps SendData to Qppc.
 *
 * \param[in]    deviceId        Peer device ID.
 * \param[in]    serviceHandle   Service handle.
 * \param[in]    length          Length of TestData to send .
 * \param[in]    testData        TestData to send .
 *
 * \return       gBleSuccess_c or error.
 ************************************************************************************/
bleResult_t Qpp_SendData(uint8_t deviceId, uint16_t serviceHandle,
                         uint16_t length, uint8_t* testData);

/*!**********************************************************************************
 * \brief        Update the GATT server with the Accessory configuration data
 *
 * \param[in]    deviceId        Peer device ID.
 * \param[in]    size            Size of the TestData
 * \param[in]    testData        TestData to send .
 *
 * \return       gBleSuccess_c or error.
 ************************************************************************************/
bleResult_t Update_Nearby(uint8_t deviceId, uint16_t size, uint8_t* testData);

/*!**********************************************************************************
 * \brief        Erase Service characteristic in GATT.
 *
 * \param[in]    deviceId        Peer device ID.
 *
 * \return       gBleSuccess_c or error.
 ************************************************************************************/
bleResult_t Erase_Nearby(uint8_t deviceId);

#ifdef __cplusplus
}
#endif

#endif /* _PRIVATE_PROFILE_INTERFACE_H_ */

/*!
 * *********************************************************************************
 * @}
 **********************************************************************************
 * */
