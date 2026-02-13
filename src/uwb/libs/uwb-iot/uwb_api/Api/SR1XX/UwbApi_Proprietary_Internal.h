/*
 *
 * Copyright 2018-2020,2022 NXP.
 *
 * NXP Confidential. This software is owned or controlled by NXP and may only be
 * used strictly in accordance with the applicable license terms. By expressly
 * accepting such terms or by downloading,installing, activating and/or
 * otherwise using the software, you are agreeing that you have read,and that
 * you agree to comply with and are bound by, such license terms. If you do not
 * agree to be bound by the applicable license terms, then you may not retain,
 * install, activate or otherwise use the software.
 *
 */

#ifndef UWBAPI_PROPRIETARY_INTERNAL_SR1XX_H
#define UWBAPI_PROPRIETARY_INTERNAL_SR1XX_H

#ifdef __cplusplus
extern "C" {
#endif

#include <UwbApi_Types_Proprietary.h>

#include "UwbApi_Types.h"
#include "phUwbTypes.h"

#if UWBFTR_SE_SN110
#include "UwbSeApi.h"
EXTERNC void reset_se_on_error(void);
#endif  // UWBFTR_SE_SN110
/*
 * below checks are required for Windows build fix and avoid coverity warning in
 * 'setDefaultAoaCalibration' .
 */
#if (UWBIOT_TML_SPI)
#if UWBIOT_UWBD_SR100T || UWBIOT_UWBD_SR100S
#define MCTT_PCTT_BIN_WITH_SSG_FW 1
#else
#define MCTT_PCTT_BIN_WITH_SSG_FW 0
#endif
#else
#define MCTT_PCTT_BIN_WITH_SSG_FW 0
#endif  // UWBIOT_HOST_PCWINDOWS
EXTERNC uint8_t getVendorAppConfigTLVBuffer(uint8_t paramId, void* paramValue,
                                            uint16_t paramValueLen,
                                            uint8_t* tlvBuffer);
EXTERNC uint8_t getVendorDebugConfigTLVBuffer(uint16_t paramId,
                                              void* paramValue,
                                              uint16_t paramValueLen,
                                              uint8_t* tlvBuffer);
EXTERNC uint8_t getExtDeviceConfigTLVBuffer(uint8_t paramId, void* paramValue,
                                            uint8_t* tlvBuffer);
EXTERNC uint8_t getExtTestConfigTLVBuffer(uint16_t paramId, void* paramValue,
                                          uint8_t* tlvBuffer);
EXTERNC void parseDebugParams(uint8_t* rspPtr, uint8_t noOfParams,
                              UWB_DebugParams_List_t* DebugParams_List);
EXTERNC void extDeviceManagementCallback(uint8_t gid, uint8_t event,
                                         uint16_t paramLength,
                                         uint8_t* pResponseBuffer);
EXTERNC tUWBAPI_STATUS
DebugConfig_TlvParser(const UWB_DebugParams_List_t* pDebugParams_List,
                      UWB_Debug_Params_value_t* pOutput_param_value);
EXTERNC BOOLEAN parseDeviceInfo(phUwbDevInfo_t* pdevInfo);
EXTERNC uint8_t getExtCoreDeviceConfigTLVBuffer(uint16_t paramId,
                                                uint8_t paramLen,
                                                void* paramValue,
                                                uint8_t* tlvBuffer);
EXTERNC void parseExtGetDeviceConfigResponse(uint8_t* tlvBuffer,
                                             phDeviceConfigData_t* devConfig);
EXTERNC tUWBAPI_STATUS setDefaultCoreConfigs(void);
#if UWBIOT_UWBD_SR150
EXTERNC void parseExtGetCalibResponse(uint8_t* tlvBuffer,
                                      phCalibRespStatus_t* calibResp);
#endif  // UWBIOT_UWBD_SR150
#if UWBFTR_Radar
EXTERNC uint8_t getExtRadarAppConfigTLVBuffer(uint16_t paramId,
                                              void* paramValue,
                                              uint16_t paramValueLen,
                                              uint8_t* tlvBuffer);
#endif  // UWBFTR_Radar//

#if UWBFTR_CCC
EXTERNC BOOLEAN parseCapabilityCCCParams(phUwbCapInfo_t* pDevCap,
                                         uint8_t paramId, uint16_t* index,
                                         uint8_t length, uint8_t* capsInfoData);
#endif  // UWBFTR_CCC

#ifdef __cplusplus
}  // closing brace for extern "C"
#endif

#endif  // UWBAPI_PROPRIETARY_INTERNAL_SR1XX_H
