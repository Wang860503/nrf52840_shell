/*
 *
 * Copyright 2020 NXP.
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

/* GENERATED FILE, DO NOT MODIFY! */

#ifndef _PHNXPLOG_UWBAPI_H
#define _PHNXPLOG_UWBAPI_H

#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

/* UWB Logging Control */
#ifndef CONFIG_UWB_LOG_ENABLED
#define CONFIG_UWB_LOG_ENABLED 0 /* Default to disabled if not defined */
#endif

#ifndef CONFIG_UWB_PRINTK_LOG
#define CONFIG_UWB_PRINTK_LOG 0 /* Default to disabled if not defined */
#endif

/* Use CONFIG_UWB_PRINTK_LOG instead of hardcoded PRINTK_LOG */
#define PRINTK_LOG CONFIG_UWB_PRINTK_LOG

#if PRINTK_LOG
#include <zephyr/sys/printk.h>
#endif

/* Global shell pointer for UWB logging */
extern const struct shell* g_uwb_shell_ptr;

#define LOG_MODULE_NAME UWBAPI
#ifndef UWB_API_MAIN_FILE
LOG_MODULE_DECLARE(UWBAPI);
#endif

/* doc:start:uci-cmd-logging */
#ifndef ENABLE_UCI_CMD_LOGGING
#ifdef NDEBUG
/* If we are in release mode, no logging */
#define ENABLE_UCI_CMD_LOGGING DISABLED  // ENABLED | DISABLED
#else
#if (UWBIOT_LOG_SILENT == 1)
/* If we are in release mode, no logging */
#define ENABLE_UCI_CMD_LOGGING DISABLED  // ENABLED | DISABLED
#else
/* If we are in debug mode, enable logging.  But then, disable during
 * development */
#define ENABLE_UCI_CMD_LOGGING ENABLED  // ENABLED | DISABLED
#endif
#endif
#endif
/* doc:end:uci-cmd-logging */

#if (UWB_GLOBAL_LOG_LEVEL >= UWB_LOG_INFO_LEVEL) && \
    (UCICORE_LOG_LEVEL >= UWB_LOG_DEBUG_LEVEL)
#define ENABLE_UCI_MODULE_TRACES TRUE
#else
#define ENABLE_UCI_MODULE_TRACES FALSE
#endif

/* Check if we are double defining these macros */
#undef LOG_E
#undef LOG_W
#undef LOG_I
#undef LOG_D

#if defined(ENABLE_UCI_CMD_LOGGING) && (ENABLE_UCI_CMD_LOGGING == ENABLED)
/* * Zephyr Hexdump API 格式: LOG_HEXDUMP_INF(data_ptr, length, message_str)
 * 這會先印出 Message，然後換行印出美觀的 Hex Dump。
 */
#define LOG_TX(Message, Array, Size) LOG_HEXDUMP_INF(Array, Size, Message)
#define LOG_RX(Message, Array, Size) LOG_HEXDUMP_INF(Array, Size, Message)
#else
/* * 如果關閉 Log，定義為空操作 (do-while 0 是標準的 C 巨集安全寫法)
 * 原本的 SPINNER() 在 Zephyr 中通常不需要，除非那是用來餵 Watchdog 的。
 */
#define LOG_TX(Message, Array, Size) \
  do {                               \
  } while (0)
#define LOG_RX(Message, Array, Size) \
  do {                               \
  } while (0)
#endif

/*
 * Use the following macros.
 */
#if !PRINTK_LOG
#if CONFIG_UWB_LOG_ENABLED
/* Helper macro to format and print to shell or fallback to printk */
#define SHELL_LOG_PRINT(fmt, ...)                 \
  do {                                            \
    const struct shell* sh_ptr = g_uwb_shell_ptr; \
    if (sh_ptr != NULL) {                         \
      shell_print(sh_ptr, fmt, ##__VA_ARGS__);    \
    } else {                                      \
      printk(fmt "\n", ##__VA_ARGS__);            \
    }                                             \
  } while (0)
#else
/* Logging disabled - define as no-op */
#define SHELL_LOG_PRINT(fmt, ...) \
  do {                            \
  } while (0)
#endif

#if CONFIG_UWB_LOG_ENABLED
#define LOG_E(...)                                \
  do {                                            \
    const struct shell* sh_ptr = g_uwb_shell_ptr; \
    if (sh_ptr != NULL) {                         \
      shell_error(sh_ptr, __VA_ARGS__);           \
    } else {                                      \
      LOG_ERR(__VA_ARGS__);                       \
    }                                             \
  } while (0)
#else
#define LOG_E(...) \
  do {             \
  } while (0)
#endif
#define LOG_X8_E(VALUE) SHELL_LOG_PRINT("E: %s=0x%02X", #VALUE, VALUE)
#define LOG_U8_E(VALUE) SHELL_LOG_PRINT("E: %s=%u", #VALUE, VALUE)
#define LOG_X16_E(VALUE) SHELL_LOG_PRINT("E: %s=0x%04X", #VALUE, VALUE)
#define LOG_U16_E(VALUE) SHELL_LOG_PRINT("E: %s=%u", #VALUE, VALUE)
#define LOG_X32_E(VALUE) SHELL_LOG_PRINT("E: %s=0x%08X", #VALUE, VALUE)
#define LOG_U32_E(VALUE) SHELL_LOG_PRINT("E: %s=%u", #VALUE, VALUE)
#if CONFIG_UWB_LOG_ENABLED
#define LOG_AU8_E(ARRAY, LEN)                     \
  do {                                            \
    const struct shell* sh_ptr = g_uwb_shell_ptr; \
    if (sh_ptr != NULL) {                         \
      shell_hexdump(sh_ptr, ARRAY, LEN);          \
    } else {                                      \
      LOG_HEXDUMP_ERR(ARRAY, LEN, #ARRAY);        \
    }                                             \
  } while (0)
#define LOG_MAU8_E(MESSAGE, ARRAY, LEN)           \
  do {                                            \
    const struct shell* sh_ptr = g_uwb_shell_ptr; \
    if (sh_ptr != NULL) {                         \
      shell_hexdump(sh_ptr, ARRAY, LEN);          \
    } else {                                      \
      LOG_HEXDUMP_ERR(ARRAY, LEN, MESSAGE);       \
    }                                             \
  } while (0)
#else
#define LOG_AU8_E(ARRAY, LEN) \
  do {                        \
  } while (0)
#define LOG_MAU8_E(MESSAGE, ARRAY, LEN) \
  do {                                  \
  } while (0)
#endif

#if CONFIG_UWB_LOG_ENABLED
#define LOG_W(...)                                \
  do {                                            \
    const struct shell* sh_ptr = g_uwb_shell_ptr; \
    if (sh_ptr != NULL) {                         \
      shell_warn(sh_ptr, __VA_ARGS__);            \
    } else {                                      \
      LOG_WRN(__VA_ARGS__);                       \
    }                                             \
  } while (0)
#else
#define LOG_W(...) \
  do {             \
  } while (0)
#endif
#define LOG_X8_W(VALUE) SHELL_LOG_PRINT("W: %s=0x%02X", #VALUE, VALUE)
#define LOG_U8_W(VALUE) SHELL_LOG_PRINT("W: %s=%u", #VALUE, VALUE)
#define LOG_X16_W(VALUE) SHELL_LOG_PRINT("W: %s=0x%04X", #VALUE, VALUE)
#define LOG_U16_W(VALUE) SHELL_LOG_PRINT("W: %s=%u", #VALUE, VALUE)
#define LOG_X32_W(VALUE) SHELL_LOG_PRINT("W: %s=0x%08X", #VALUE, VALUE)
#define LOG_U32_W(VALUE) SHELL_LOG_PRINT("W: %s=%u", #VALUE, VALUE)
#if CONFIG_UWB_LOG_ENABLED
#define LOG_AU8_W(ARRAY, LEN)                     \
  do {                                            \
    const struct shell* sh_ptr = g_uwb_shell_ptr; \
    if (sh_ptr != NULL) {                         \
      shell_hexdump(sh_ptr, ARRAY, LEN);          \
    } else {                                      \
      LOG_HEXDUMP_WRN(ARRAY, LEN, #ARRAY);        \
    }                                             \
  } while (0)
#define LOG_MAU8_W(MESSAGE, ARRAY, LEN)           \
  do {                                            \
    const struct shell* sh_ptr = g_uwb_shell_ptr; \
    if (sh_ptr != NULL) {                         \
      shell_hexdump(sh_ptr, ARRAY, LEN);          \
    } else {                                      \
      LOG_HEXDUMP_WRN(ARRAY, LEN, MESSAGE);       \
    }                                             \
  } while (0)
#else
#define LOG_AU8_W(ARRAY, LEN) \
  do {                        \
  } while (0)
#define LOG_MAU8_W(MESSAGE, ARRAY, LEN) \
  do {                                  \
  } while (0)
#endif

#if CONFIG_UWB_LOG_ENABLED
#define LOG_I(...)                                \
  do {                                            \
    const struct shell* sh_ptr = g_uwb_shell_ptr; \
    if (sh_ptr != NULL) {                         \
      shell_print(sh_ptr, __VA_ARGS__);           \
    } else {                                      \
      LOG_INF(__VA_ARGS__);                       \
    }                                             \
  } while (0)
#else
#define LOG_I(...) \
  do {             \
  } while (0)
#endif
#define LOG_X8_I(VALUE) SHELL_LOG_PRINT("I: %s=0x%02X", #VALUE, VALUE)
#define LOG_U8_I(VALUE) SHELL_LOG_PRINT("I: %s=%u", #VALUE, VALUE)
#define LOG_X16_I(VALUE) SHELL_LOG_PRINT("I: %s=0x%04X", #VALUE, VALUE)
#define LOG_U16_I(VALUE) SHELL_LOG_PRINT("I: %s=%u", #VALUE, VALUE)
#define LOG_X32_I(VALUE) SHELL_LOG_PRINT("I: %s=0x%08X", #VALUE, VALUE)
#define LOG_U32_I(VALUE) SHELL_LOG_PRINT("I: %s=%u", #VALUE, VALUE)
#if CONFIG_UWB_LOG_ENABLED
#define LOG_AU8_I(ARRAY, LEN)                     \
  do {                                            \
    const struct shell* sh_ptr = g_uwb_shell_ptr; \
    if (sh_ptr != NULL) {                         \
      shell_hexdump(sh_ptr, ARRAY, LEN);          \
    } else {                                      \
      LOG_HEXDUMP_INF(ARRAY, LEN, #ARRAY);        \
    }                                             \
  } while (0)
#define LOG_MAU8_I(MESSAGE, ARRAY, LEN)           \
  do {                                            \
    const struct shell* sh_ptr = g_uwb_shell_ptr; \
    if (sh_ptr != NULL) {                         \
      shell_hexdump(sh_ptr, ARRAY, LEN);          \
    } else {                                      \
      LOG_HEXDUMP_INF(ARRAY, LEN, MESSAGE);       \
    }                                             \
  } while (0)
#else
#define LOG_AU8_I(ARRAY, LEN) \
  do {                        \
  } while (0)
#define LOG_MAU8_I(MESSAGE, ARRAY, LEN) \
  do {                                  \
  } while (0)
#endif

#if CONFIG_UWB_LOG_ENABLED
#define LOG_D(...)                                \
  do {                                            \
    const struct shell* sh_ptr = g_uwb_shell_ptr; \
    if (sh_ptr != NULL) {                         \
      shell_print(sh_ptr, __VA_ARGS__);           \
    } else {                                      \
      LOG_DBG(__VA_ARGS__);                       \
    }                                             \
  } while (0)
#else
#define LOG_D(...) \
  do {             \
  } while (0)
#endif
#define LOG_X8_D(VALUE) SHELL_LOG_PRINT("D: %s=0x%02X", #VALUE, VALUE)
#define LOG_U8_D(VALUE) SHELL_LOG_PRINT("D: %s=%u", #VALUE, VALUE)
#define LOG_X16_D(VALUE) SHELL_LOG_PRINT("D: %s=0x%04X", #VALUE, VALUE)
#define LOG_U16_D(VALUE) SHELL_LOG_PRINT("D: %s=%u", #VALUE, VALUE)
#define LOG_X32_D(VALUE) SHELL_LOG_PRINT("D: %s=0x%08X", #VALUE, VALUE)
#define LOG_U32_D(VALUE) SHELL_LOG_PRINT("D: %s=%u", #VALUE, VALUE)
#if CONFIG_UWB_LOG_ENABLED
#define LOG_AU8_D(ARRAY, LEN)                     \
  do {                                            \
    const struct shell* sh_ptr = g_uwb_shell_ptr; \
    if (sh_ptr != NULL) {                         \
      shell_hexdump(sh_ptr, ARRAY, LEN);          \
    } else {                                      \
      LOG_HEXDUMP_DBG(ARRAY, LEN, #ARRAY);        \
    }                                             \
  } while (0)
#define LOG_MAU8_D(MESSAGE, ARRAY, LEN)           \
  do {                                            \
    const struct shell* sh_ptr = g_uwb_shell_ptr; \
    if (sh_ptr != NULL) {                         \
      shell_hexdump(sh_ptr, ARRAY, LEN);          \
    } else {                                      \
      LOG_HEXDUMP_DBG(ARRAY, LEN, MESSAGE);       \
    }                                             \
  } while (0)
#else
#define LOG_AU8_D(ARRAY, LEN) \
  do {                        \
  } while (0)
#define LOG_MAU8_D(MESSAGE, ARRAY, LEN) \
  do {                                  \
  } while (0)
#endif

#else
/* Helper Macro: 用於 Hex Dump 的通用迴圈實作 */
/* ------------------------------------------------------------------------- */
#define PRINTK_HEXDUMP(PREFIX, MSG, PTR, LEN)           \
  do {                                                  \
    printk("%s%s (len=%d): ", PREFIX, MSG, (int)(LEN)); \
    const uint8_t* p = (const uint8_t*)(PTR);           \
    for (int i = 0; i < (LEN); i++) {                   \
      printk("%02X ", p[i]);                            \
    }                                                   \
    printk("\n");                                       \
  } while (0)

/* ------------------------------------------------------------------------- */
/* Error Level (E) */
/* ------------------------------------------------------------------------- */
#if CONFIG_UWB_LOG_ENABLED
#define LOG_E(fmt, ...)         \
  do {                          \
    printk("E: ");              \
    printk(fmt, ##__VA_ARGS__); \
    printk("\n");               \
  } while (0)
#define LOG_X8_E(VALUE) printk("E: %s=0x%02X\n", #VALUE, VALUE)
#define LOG_U8_E(VALUE) printk("E: %s=%u\n", #VALUE, VALUE)
#define LOG_X16_E(VALUE) printk("E: %s=0x%04X\n", #VALUE, VALUE)
#define LOG_U16_E(VALUE) printk("E: %s=%u\n", #VALUE, VALUE)
#define LOG_X32_E(VALUE) printk("E: %s=0x%08X\n", #VALUE, VALUE)
#define LOG_U32_E(VALUE) printk("E: %s=%u\n", #VALUE, VALUE)

/* Hex Dump Error */
#define LOG_AU8_E(ARRAY, LEN) PRINTK_HEXDUMP("E: ", #ARRAY, ARRAY, LEN)
#define LOG_MAU8_E(MESSAGE, ARRAY, LEN) \
  PRINTK_HEXDUMP("E: ", MESSAGE, ARRAY, LEN)
#else
#define LOG_E(fmt, ...) \
  do {                  \
  } while (0)
#define LOG_X8_E(VALUE) \
  do {                  \
  } while (0)
#define LOG_U8_E(VALUE) \
  do {                  \
  } while (0)
#define LOG_X16_E(VALUE) \
  do {                   \
  } while (0)
#define LOG_U16_E(VALUE) \
  do {                   \
  } while (0)
#define LOG_X32_E(VALUE) \
  do {                   \
  } while (0)
#define LOG_U32_E(VALUE) \
  do {                   \
  } while (0)
#define LOG_AU8_E(ARRAY, LEN) \
  do {                        \
  } while (0)
#define LOG_MAU8_E(MESSAGE, ARRAY, LEN) \
  do {                                  \
  } while (0)
#endif

/* ------------------------------------------------------------------------- */
/* Warning Level (W) */
/* ------------------------------------------------------------------------- */
#if CONFIG_UWB_LOG_ENABLED
#define LOG_W(fmt, ...)         \
  do {                          \
    printk("W: ");              \
    printk(fmt, ##__VA_ARGS__); \
    printk("\n");               \
  } while (0)
#define LOG_X8_W(VALUE) printk("W: %s=0x%02X\n", #VALUE, VALUE)
#define LOG_U8_W(VALUE) printk("W: %s=%u\n", #VALUE, VALUE)
#define LOG_X16_W(VALUE) printk("W: %s=0x%04X\n", #VALUE, VALUE)
#define LOG_U16_W(VALUE) printk("W: %s=%u\n", #VALUE, VALUE)
#define LOG_X32_W(VALUE) printk("W: %s=0x%08X\n", #VALUE, VALUE)
#define LOG_U32_W(VALUE) printk("W: %s=%u\n", #VALUE, VALUE)

/* Hex Dump Warning */
#define LOG_AU8_W(ARRAY, LEN) PRINTK_HEXDUMP("W: ", #ARRAY, ARRAY, LEN)
#define LOG_MAU8_W(MESSAGE, ARRAY, LEN) \
  PRINTK_HEXDUMP("W: ", MESSAGE, ARRAY, LEN)
#else
#define LOG_W(fmt, ...) \
  do {                  \
  } while (0)
#define LOG_X8_W(VALUE) \
  do {                  \
  } while (0)
#define LOG_U8_W(VALUE) \
  do {                  \
  } while (0)
#define LOG_X16_W(VALUE) \
  do {                   \
  } while (0)
#define LOG_U16_W(VALUE) \
  do {                   \
  } while (0)
#define LOG_X32_W(VALUE) \
  do {                   \
  } while (0)
#define LOG_U32_W(VALUE) \
  do {                   \
  } while (0)
#define LOG_AU8_W(ARRAY, LEN) \
  do {                        \
  } while (0)
#define LOG_MAU8_W(MESSAGE, ARRAY, LEN) \
  do {                                  \
  } while (0)
#endif

/* ------------------------------------------------------------------------- */
/* Info Level (I) */
/* ------------------------------------------------------------------------- */
#if CONFIG_UWB_LOG_ENABLED
#define LOG_I(fmt, ...)         \
  do {                          \
    printk("I: ");              \
    printk(fmt, ##__VA_ARGS__); \
    printk("\n");               \
  } while (0)
#define LOG_X8_I(VALUE) printk("I: %s=0x%02X\n", #VALUE, VALUE)
#define LOG_U8_I(VALUE) printk("I: %s=%u\n", #VALUE, VALUE)
#define LOG_X16_I(VALUE) printk("I: %s=0x%04X\n", #VALUE, VALUE)
#define LOG_U16_I(VALUE) printk("I: %s=%u\n", #VALUE, VALUE)
#define LOG_X32_I(VALUE) printk("I: %s=0x%08X\n", #VALUE, VALUE)
#define LOG_U32_I(VALUE) printk("I: %s=%u\n", #VALUE, VALUE)

/* Hex Dump Info */
#define LOG_AU8_I(ARRAY, LEN) PRINTK_HEXDUMP("I: ", #ARRAY, ARRAY, LEN)
#define LOG_MAU8_I(MESSAGE, ARRAY, LEN) \
  PRINTK_HEXDUMP("I: ", MESSAGE, ARRAY, LEN)
#else
#define LOG_I(fmt, ...) \
  do {                  \
  } while (0)
#define LOG_X8_I(VALUE) \
  do {                  \
  } while (0)
#define LOG_U8_I(VALUE) \
  do {                  \
  } while (0)
#define LOG_X16_I(VALUE) \
  do {                   \
  } while (0)
#define LOG_U16_I(VALUE) \
  do {                   \
  } while (0)
#define LOG_X32_I(VALUE) \
  do {                   \
  } while (0)
#define LOG_U32_I(VALUE) \
  do {                   \
  } while (0)
#define LOG_AU8_I(ARRAY, LEN) \
  do {                        \
  } while (0)
#define LOG_MAU8_I(MESSAGE, ARRAY, LEN) \
  do {                                  \
  } while (0)
#endif

/* ------------------------------------------------------------------------- */
/* Debug Level (D) */
/* ------------------------------------------------------------------------- */
#if CONFIG_UWB_LOG_ENABLED
#define LOG_D(fmt, ...)         \
  do {                          \
    printk("D: ");              \
    printk(fmt, ##__VA_ARGS__); \
    printk("\n");               \
  } while (0)
#define LOG_X8_D(VALUE) printk("D: %s=0x%02X\n", #VALUE, VALUE)
#define LOG_U8_D(VALUE) printk("D: %s=%u\n", #VALUE, VALUE)
#define LOG_X16_D(VALUE) printk("D: %s=0x%04X\n", #VALUE, VALUE)
#define LOG_U16_D(VALUE) printk("D: %s=%u\n", #VALUE, VALUE)
#define LOG_X32_D(VALUE) printk("D: %s=0x%08X\n", #VALUE, VALUE)
#define LOG_U32_D(VALUE) printk("D: %s=%u\n", #VALUE, VALUE)

/* Hex Dump Debug */
#define LOG_AU8_D(ARRAY, LEN) PRINTK_HEXDUMP("D: ", #ARRAY, ARRAY, LEN)
#define LOG_MAU8_D(MESSAGE, ARRAY, LEN) \
  PRINTK_HEXDUMP("D: ", MESSAGE, ARRAY, LEN)
#else
#define LOG_D(fmt, ...) \
  do {                  \
  } while (0)
#define LOG_X8_D(VALUE) \
  do {                  \
  } while (0)
#define LOG_U8_D(VALUE) \
  do {                  \
  } while (0)
#define LOG_X16_D(VALUE) \
  do {                   \
  } while (0)
#define LOG_U16_D(VALUE) \
  do {                   \
  } while (0)
#define LOG_X32_D(VALUE) \
  do {                   \
  } while (0)
#define LOG_U32_D(VALUE) \
  do {                   \
  } while (0)
#define LOG_AU8_D(ARRAY, LEN) \
  do {                        \
  } while (0)
#define LOG_MAU8_D(MESSAGE, ARRAY, LEN) \
  do {                                  \
  } while (0)
#endif
#endif
/*
 * These are the Legacy macros used for logging.
 * Do not use further.
 */
#define NXPLOG_UWBAPI_E LOG_E
#define NXPLOG_UWBAPI_W LOG_W
#define NXPLOG_UWBAPI_I LOG_I
#define NXPLOG_UWBAPI_D LOG_D

#define NXPLOG_APP_D LOG_D
#define NXPLOG_APP_I LOG_I
#define NXPLOG_APP_E LOG_E
#define NXPLOG_APP_W LOG_W

#define UCI_TRACE_E LOG_E
#define UCI_TRACE_W LOG_W
#define UCI_TRACE_I LOG_I
#define UCI_TRACE_D LOG_D

#define NXPLOG_UCIX_E LOG_E
#define NXPLOG_UCIX_W LOG_W
#define NXPLOG_UCIX_I LOG_I
#define NXPLOG_UCIX_D LOG_D

/*
 * These are the Legacy macros used for logging.
 * Do not use further.
 */
#define NXPLOG_UWB_TML_E LOG_E
#define NXPLOG_UWB_TML_W LOG_W
#define NXPLOG_UWB_TML_I LOG_I
#define NXPLOG_UWB_TML_D LOG_D

/*
 * These are the Legacy macros used for logging.
 * Do not use further.
 */
#define NXPLOG_UCIX_E LOG_E
#define NXPLOG_UCIX_W LOG_W
#define NXPLOG_UCIX_I LOG_I
#define NXPLOG_UCIX_D LOG_D

/*
 * These are the Legacy macros used for logging.
 * Do not use further.
 */
#define NXPLOG_UCIHAL_E LOG_E
#define NXPLOG_UCIHAL_W LOG_W
#define NXPLOG_UCIHAL_I LOG_I
#define NXPLOG_UCIHAL_D LOG_D

/*
 * These are the Legacy macros used for logging.
 * Do not use further.
 */
#define NXPLOG_UWB_FWDNLD_E LOG_E
#define NXPLOG_UWB_FWDNLD_W LOG_W
#define NXPLOG_UWB_FWDNLD_I LOG_I
#define NXPLOG_UWB_FWDNLD_D LOG_D

/*
 * These are the Legacy macros used for logging.
 * Do not use further.
 */
#define UCI_TRACE_E LOG_E
#define UCI_TRACE_W LOG_W
#define UCI_TRACE_I LOG_I
#define UCI_TRACE_D LOG_D
#endif /* _PHNXPLOG_UWBAPI_H */
