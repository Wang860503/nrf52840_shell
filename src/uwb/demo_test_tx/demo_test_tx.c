/* Copyright 2021,2023 NXP
 *
 * NXP Confidential. This software is owned or controlled by NXP and may only
 * be used strictly in accordance with the applicable license terms.  By
 * expressly accepting such terms or by downloading, installing, activating
 * and/or otherwise using the software, you are agreeing that you have read,
 * and that you agree to comply with and are bound by, such license terms.  If
 * you do not agree to be bound by the applicable license terms, then you may
 * not retain, install, activate or otherwise use the software.
 */

#include "phUwb_BuildConfig.h"

#ifndef UWBIOT_APP_BUILD__DEMO_TEST_TX
#include "UWBIOT_APP_BUILD.h"
#endif

#ifdef UWBIOT_APP_BUILD__DEMO_TEST_TX

#include <zephyr/shell/shell.h>
#include <zephyr/sys/printk.h>

#include "AppInternal.h"
#include "AppRecovery.h"
#include "UwbApi.h"
#include "UwbApi_Internal.h"
#include "UwbApi_RfTest.h"
#include "app_Test_Cfg.h"
#include "demo_test_tx.h"
#include "phOsalUwb.h"

K_SEM_DEFINE(uwb_test_tx, 1, 1);

/* Define launch_time here (declared as extern in demo_test_tx.h) */
uint32_t launch_time = 10;

#define DEMO_NUM_PACKETS (1000 * 10)
#define DEMO_T_GAP (1000)
#define DEMO_T_START (450)
#define DEMO_T_WIN (750)
/* DEMO_RANDOMIZE_OR_FIXED_PSDU
 * 0 = fixed
 * 1 = random
 */
#define DEMO_RANDOMIZE_OR_FIXED_PSDU 0

#define DEMO_TEST_TX_TASK_SIZE 400
#define DEMO_TEST_TX_TASK_NAME "DemoTestTx"
#define DEMO_TEST_TX_TASK_PRIO 4

phRfTestParams_t rfTestParams = {
    /* Ok */
    .numOfPckts = DEMO_NUM_PACKETS,
    .tGap = DEMO_T_GAP,
    .tStart = DEMO_T_START,
    .tWin = DEMO_T_WIN,
    .randomizedSize = DEMO_RANDOMIZE_OR_FIXED_PSDU,
    .phrRangingBit = 0,
    .rmarkerRxStart = 0,
    .rmarkerTxStart = 0,
    .stsIndexAutoIncr = 0,
};

/* Select the channel ID we want to use */
static const uint8_t gkchannelId = DEMO_RF_TEST_CHANNEL_ID;

/* Select what power we want to send at.
 * - 0 => 14dbm MAX
 * - 104 => -12dbm Minimum
 */
const uint8_t gtxPower = 24;

/*  [9-24]:BPRF, [25-32]:HPRF
 *
 * 10 default in Helios Test modes
 */
static const uint8_t gkPreambleCodeIndex = DEMO_RF_TEST_PREAMBLE_CODE_INDEX;

/*  [0,2]:BPRF, [1,3]:HPRF */
static const uint8_t gkSfdId = DEMO_RF_TEST_SFD_ID;

const UWB_RfFrameConfig_t rframeConfig = DEMO_RF_TEST_RFRAME_CONFIG;
/*
 * Below list contains the application configs which are only related to default
 * configuration.
 */
void Demo_Test_Tx(void) {
  tUWBAPI_STATUS status = UWBAPI_STATUS_FAILED;
  phRfStartData_t startData;
  phUwbDevInfo_t devInfo;
  uint32_t delay = 5;
  uint32_t sessionHandle = 0;
  UWB_AppParams_List_t SetAppParamsList[] = {
      UWB_SET_APP_PARAM_VALUE(CHANNEL_NUMBER, gkchannelId),
      UWB_SET_APP_PARAM_VALUE(SFD_ID, gkSfdId),
      UWB_SET_APP_PARAM_VALUE(PREAMBLE_CODE_INDEX, gkPreambleCodeIndex),
      UWB_SET_APP_PARAM_VALUE(RFRAME_CONFIG, rframeConfig),
      UWB_SET_APP_PARAM_VALUE(PSDU_DATA_RATE, DEMO_RF_TEST_PSDU_DATA_RATE),
      UWB_SET_APP_PARAM_VALUE(PREAMBLE_DURATION,
                              DEMO_RF_TEST_PREAMBLE_DURATION),
      UWB_SET_APP_PARAM_VALUE(PRF_MODE, DEMO_RF_TEST_PRF_MODE),
  };

  GENERATE_SEND_DATA(dataToSend, PSDU_DATA_SIZE)

  /* PRINT_APP_NAME is now called in uwb_tx_thread_entry before UWBDemo_Init()
   */

  /* Initialize the UWB Middleware */

  NXPLOG_APP_I("[Demo_Test_Tx] Calling UwbApi_Init...");
  status = UwbApi_Init(AppCallback);
  if (status != UWBAPI_STATUS_OK) {
    NXPLOG_APP_E("[Demo_Test_Tx] ERROR: UwbApi_Init Failed (status=%d)",
                 status);
    /* Clear global shell pointer before error handling */
    extern const struct shell* g_uwb_shell_ptr;
    g_uwb_shell_ptr = NULL;
    goto exit;
  }
  phOsalUwb_Delay(delay);
  NXPLOG_APP_I("[Demo_Test_Tx] UwbApi_Init OK");

  NXPLOG_APP_I("[Demo_Test_Tx] Calling UwbApi_GetDeviceInfo...");
  status = UwbApi_GetDeviceInfo(&devInfo);
  NXPLOG_APP_I("[Demo_Test_Tx] UwbApi_GetDeviceInfo returned status=%d",
               status);
  if (status != UWBAPI_STATUS_OK) {
    NXPLOG_APP_E(
        "[Demo_Test_Tx] ERROR: UwbApi_GetDeviceInfo() Failed (status=%d)",
        status);
    NXPLOG_APP_E("UwbApi_GetDeviceInfo() Failed");
    goto exit;
  }
  phOsalUwb_Delay(delay);
  NXPLOG_APP_I("[Demo_Test_Tx] Calling printDeviceInfo...");
  /* Temporarily skip printDeviceInfo to avoid blocking */
  /* printDeviceInfo(&devInfo); */
  NXPLOG_APP_I("[Demo_Test_Tx] printDeviceInfo skipped (FW: %02X.%02X.%02X)",
               devInfo.fwMajor, devInfo.fwMinor, devInfo.fwRc);
  NXPLOG_APP_I("[Demo_Test_Tx] UwbApi_GetDeviceInfo OK");

  NXPLOG_APP_I("[Demo_Test_Tx] Calling UwbApi_SessionInit...");
  status = UwbApi_SessionInit(SESSION_ID_RFTEST, UWBD_RFTEST, &sessionHandle);
  if (status != UWBAPI_STATUS_OK) {
    NXPLOG_APP_E(
        "[Demo_Test_Tx] ERROR: UwbApi_SessionInit() Failed (status=%d)",
        status);
    goto exit;
  }
  phOsalUwb_Delay(delay);
  NXPLOG_APP_I("[Demo_Test_Tx] UwbApi_SessionInit OK (sessionHandle=%u)",
               sessionHandle);

  NXPLOG_APP_I("[Demo_Test_Tx] Calling UwbApi_SetRfTestParams...");
  status = UwbApi_SetRfTestParams(sessionHandle, &rfTestParams);
  if (status != UWBAPI_STATUS_OK) {
    NXPLOG_APP_E(
        "[Demo_Test_Tx] ERROR: UwbApi_SetRfTestParams() Failed (status=%d)",
        status);
    goto exit;
  }
  phOsalUwb_Delay(delay);
  NXPLOG_APP_I("[Demo_Test_Tx] UwbApi_SetRfTestParams OK");

  NXPLOG_APP_I("[Demo_Test_Tx] Calling UwbApi_SetAppConfigMultipleParams...");
  status = UwbApi_SetAppConfigMultipleParams(
      sessionHandle, sizeof(SetAppParamsList) / sizeof(SetAppParamsList[0]),
      &SetAppParamsList[0]);
  if (status != UWBAPI_STATUS_OK) {
    NXPLOG_APP_E(
        "[Demo_Test_Tx] ERROR: UwbApi_SetAppConfigMultipleParams() Failed "
        "(status=%d)",
        status);
    goto exit;
  }
  phOsalUwb_Delay(delay);
  NXPLOG_APP_I("[Demo_Test_Tx] UwbApi_SetAppConfigMultipleParams OK");

  NXPLOG_APP_I(
      "[Demo_Test_Tx] Calling UwbApi_SetAppConfig "
      "(NUMBER_OF_STS_SEGMENTS)...");
  status = UwbApi_SetAppConfig(sessionHandle, NUMBER_OF_STS_SEGMENTS,
                               DEMO_RF_TEST_NUMBER_OF_STS_SEGMENTS);
  if (status != UWBAPI_STATUS_OK) {
    NXPLOG_APP_E(
        "[Demo_Test_Tx] ERROR: UwbApi_SetAppConfig() for "
        "NUMBER_OF_STS_SEGMENTS Failed (status=%d)",
        status);
    goto exit;
  }
  phOsalUwb_Delay(delay);
  NXPLOG_APP_I(
      "[Demo_Test_Tx] UwbApi_SetAppConfig (NUMBER_OF_STS_SEGMENTS) OK");

  NXPLOG_APP_I("[Demo_Test_Tx] Calling UwbApi_SetTestConfig...");
  status = UwbApi_SetTestConfig(sessionHandle, TEST_SESSION_STS_KEY_OPTION,
                                1 /* IEEE Keys == 1 */);
  if (status != UWBAPI_STATUS_OK) {
    NXPLOG_APP_E(
        "[Demo_Test_Tx] ERROR: UwbApi_SetTestConfig() Failed (status=%d)",
        status);
    goto exit;
  }
  phOsalUwb_Delay(delay);
  NXPLOG_APP_I("[Demo_Test_Tx] UwbApi_SetTestConfig OK");

  NXPLOG_APP_I("[Demo_Test_Tx] Calling UwbApi_StartRfTest...");
  startData.startPerTxData.txDataLength = PSDU_DATA_SIZE;
  startData.startPerTxData.txData = dataToSend;
  status = UwbApi_StartRfTest(RF_START_PER_TX, &startData);
  if (status != UWBAPI_STATUS_OK) {
    NXPLOG_APP_E(
        "[Demo_Test_Tx] ERROR: UwbApi_StartRfTest() Failed (status=%d)",
        status);
    goto exit;
  }
  NXPLOG_APP_I("[Demo_Test_Tx] UwbApi_StartRfTest OK - RF test started!");

  /* Delay 1 Min for Ranging MILLISECONDS = MINUTES * 60 * 1000 */
#if CONFIG_UWB_LOG_ENABLED
  NXPLOG_APP_I("Sleeping for %ds", launch_time);
#else
  printk("Sleeping for %ds\n", launch_time);
#endif
  phOsalUwb_Delay(launch_time * 1000);

  NXPLOG_APP_I("[Demo_Test_Tx] Calling UwbApi_Stop_RfTest...");
  status = UwbApi_Stop_RfTest();
  if (status != UWBAPI_STATUS_OK) {
    if (status == UWBAPI_STATUS_REJECTED) {
      NXPLOG_APP_W("[Demo_Test_Tx] WARNING: UwbApi_Stop_RfTest()");
    }
  } else {
    NXPLOG_APP_I("[Demo_Test_Tx] UwbApi_Stop_RfTest OK");
  }
  NXPLOG_APP_I("[Demo_Test_Tx] Calling UwbApi_SessionDeinit...");
  status = UwbApi_SessionDeinit(sessionHandle);
  if (status != UWBAPI_STATUS_OK) {
    NXPLOG_APP_E(
        "[Demo_Test_Tx] ERROR: UwbApi_SessionDeinit() Failed (status=%d)",
        status);
    /* Continue anyway - test was successful */
  } else {
    NXPLOG_APP_I("[Demo_Test_Tx] UwbApi_SessionDeinit OK");
  }
exit:
  /* Clear global shell pointer before error handling to avoid Bus Fault */
  extern const struct shell* g_uwb_shell_ptr;
  g_uwb_shell_ptr = NULL;

  if (status == UWBAPI_STATUS_TIMEOUT) {
    Handle_ErrorScenario(TIME_OUT);
  } else if (status == UWBAPI_STATUS_HPD_WAKEUP) {
    /* This must after HPD. Device Reset is must to come out of HPD*/
    UWBIOT_EXAMPLE_END(status);
  }
  UWBIOT_EXAMPLE_END(status);
}

#endif  // UWBIOT_APP_BUILD__DEMO_TEST_TX
