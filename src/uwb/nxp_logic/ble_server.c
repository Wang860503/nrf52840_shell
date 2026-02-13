/* TLV includes */
#include "TLV_Types_i.h"
#include "stdio.h"

/************************************************************************************
*************************************************************************************
* Private functions prototypes
*************************************************************************************
************************************************************************************/

/* Gatt and Att callbacks */
#ifdef UWBIOT_APP_BUILD__DEMO_NEARBY_INTERACTION

void BleApp_ReceivedDataHandler(deviceId_t deviceId, uint8_t* aValue,
                                uint16_t valueLength) {
  tlvRecv((uint8_t)deviceId, UWB_HIF_BLE, aValue, valueLength);
}

#endif