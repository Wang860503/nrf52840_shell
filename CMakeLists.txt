# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(hello_world)

target_sources_ifndef(CONFIG_DTM_TRANSPORT_HCI app PRIVATE
  src/dtm/transport/dtm_uart_twowire.c
  src/dtm/transport/dtm_uart_wait.c
)

target_sources_ifdef(CONFIG_DTM_TRANSPORT_HCI app PRIVATE src/dtm/transport/dtm_hci.c)

if(CONFIG_DTM_TRANSPORT_HCI AND "${BOARD}" STREQUAL "nrf5340dk")
  target_sources(app PRIVATE src/dtm/transport/hci_uart_remote.c)
  target_include_directories(app PRIVATE ./rpc)
else()
  target_sources_ifdef(CONFIG_DTM_TRANSPORT_HCI app PRIVATE src/dtm/transport/hci_uart.c)
endif()

target_include_directories(app PRIVATE
    src/buzzer
    src/radio_test
    src/pn7160
    src/pn7160/inc
    src/pn7160/TML/inc
    src/pn7160/board/inc
    src/pn7160/NfcLibrary_NCI2.0/inc
    src/pn7160/NfcLibrary_NCI2.0/NxpNci20/inc
    src/pn7160/NfcLibrary_NCI2.0/NdefLibrary/inc
    src/dtm/
    src/dtm/transport
    src/em4095/
    src/uwb/common/
    src/uwb/nxp_logic/inc
    src/uwb/ext/firmware_images/SR1XX/
    src/uwb/ext/boards/nrf52840dk/bluetooth/profiles/private_profile/
    src/uwb/libs/halimpl/inc/
    src/uwb/libs/halimpl/fwd/
    src/uwb/libs/halimpl/transport/SPI/
    src/uwb/libs/uci-core/inc/
    src/uwb/libs/uci-core/inc/SR1XX/
    src/uwb/libs/uwb-iot/uwb_core/include/
    src/uwb/libs/uwb-iot/uwb_api/Api/
    src/uwb/libs/uwb-iot/uwb_api/Api/SR1xx/
    src/uwb/libs/uwb-iot/uwb_api/Api/SR1xx/SR150/
    src/uwb/libs/halimpl/fwd/SR1XX/
    src/uwb/boards/Host/Rhodes4/
    src/uwb/boards/Rhodes4_SPI/
    src/uwb/se_mw/se_hostlib/hostLib/fira/inc/
    src/uwb/se_mw/se_hostlib/hostLib/inc/
    src/uwb/se_mw/se_hostlib/hostLib/libCommon/infra/
    src/uwb/se_mw/se_hostlib/hostLib/libCommon/log/
    src/uwb/se_mw/se_hostlib/hostLib/libCommon/smCom/
    src/uwb/se_mw/sss/inc/
    src/uwb/se_mw/sss/ex/inc/
    src/uwb/se_mw/sss/port/default/
    src/uwb/demos/common/
    src/uwb/firmware_images/SR1XX/Prod_ROW/
    src/uwb/firmware_images/SR1XX/
    src/uwb/demo_test_rx/
    src/uwb/demo_test_tx/
)

# Application sources
FILE(GLOB app_sources 
    src/*.c 
    src/cmd/*.c
    src/buzzer/*.c 
    src/radio_test/*.c
    src/pn7160/*.c
    src/pn7160/TML/src/*.c
    src/pn7160/NfcLibrary_NCI2.0/NxpNci20/src/*.c
    src/pn7160/NfcLibrary_NCI2.0/NdefLibrary/src/*.c
    src/dtm/*.c
    src/em4095/*.c
    src/uwb/*.c
)

FILE(GLOB_RECURSE uwb_sources
    src/uwb/*/*.c
)

target_sources(app PRIVATE 
    ${app_sources}
    ${uwb_sources}
)

target_sources(app PRIVATE 
    ${app_sources}
)

target_compile_definitions(app PRIVATE 
    #BUZZER_SUPPORT 
    RW_SUPPORT     # pn7160 
    senao # UWB antenna senao config
    UWBIOT_APP_BUILD__DEMO_TEST_TX
    #UWBIOT_APP_BUILD__DEMO_TEST_RX
)