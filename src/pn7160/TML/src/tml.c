/*
 *         Copyright (c), NXP Semiconductors Caen / France
 *
 *                     (C)NXP Semiconductors
 *       All rights are reserved. Reproduction in whole or in part is
 *      prohibited without the written consent of the copyright owner.
 *  NXP reserves the right to make changes without notice at any time.
 * NXP makes no warranty, expressed, implied or statutory, including but
 * not limited to any implied warranty of merchantability or fitness for any
 *particular purpose, or that the use will not infringe any third party patent,
 * copyright or trademark. NXP must not be liable for any loss or damage
 *                          arising from its use.
 */

#include <stdint.h>
#include <tool.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>

#include "driver_config.h"
#include "types.h"

static uint8_t tml_Init(void) {
  /* I2C */
  if (!device_is_ready(pn7160_i2c.bus)) {
    printk("[PN7150] I2C bus %s is not ready!\n\r", pn7160_i2c.bus->name);
    return -1;
  } else {
    printk("[PN7150] I2C bus %s is ready!\n\r", pn7160_i2c.bus->name);
  }
  /* Configure GPIO for IRQ pin */
  if (device_is_ready(pn7160_irq.port)) {
    int ret = gpio_pin_configure_dt(&pn7160_irq, GPIO_INPUT);
    if (ret < 0) {
      printk("[PN7150] IRQ setting input fail\n\r");
    }
    printk("[PN7150] IRQ setting input Success\n\r");
  }
  /* Configure GPIO for RESET pin */
  if (device_is_ready(pn7160_reset.port)) {
    int ret = gpio_pin_configure_dt(&pn7160_reset, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
      printk("[PN7150] RESET setting ouput fail\n\r");
    }
    printk("[PN7150] RESET setting ouput Success\n\r");
  }

  return SUCCESS;
}

static uint8_t tml_Reset(void) {
  /* Apply VEN reset */
  gpio_pin_set_dt(&pn7160_reset, HIGH);
  k_msleep(10);
  gpio_pin_set_dt(&pn7160_reset, LOW);
  k_msleep(10);
  gpio_pin_set_dt(&pn7160_reset, HIGH);
  printk("[PN7150] RESET Success\n\r");
  return SUCCESS;
}

static uint8_t tml_Tx(uint8_t* pBuff, uint16_t buffLen) {
  if (i2c_write_dt(&pn7160_i2c, pBuff, buffLen) != SUCCESS) {
    k_msleep(10);
    printk("%s i2c_write fail retry\n\r", __func__);
    if (i2c_write_dt(&pn7160_i2c, pBuff, buffLen) != SUCCESS) {
      return ERROR;
    }
  }
  // printk("%s i2c_write success\n\r", __func__);
  return SUCCESS;
}

static uint8_t tml_Rx(uint8_t* pBuff, uint16_t buffLen, uint16_t* pBytesRead) {
  uint8_t ret;
  /* DS page 21, split mode  to read dat from PN7150 */
  /* 1. 1st frame, ADDR, NICHeader1, NICHeader2, payload length, total 4 byte */
  /* 2. 2nd frame, ADDR, payload legth bytes */
  /* pBuff[3] : read data saving pointer */
  /* pBuff[2] : length to read */
  ret = i2c_read_dt(&pn7160_i2c, pBuff, 3);

  if (ret == 0) {
    /* pBuff[2] 是 Payload Length */
    uint8_t payload_len = pBuff[2];

    if (payload_len > 0) {
      /* --- 安全檢查 (選用，但在 C 語言很重要) --- */
      if ((3 + payload_len) > buffLen) {
        // 避免 Buffer Overflow
        *pBytesRead = 0;
        return -1;
      }
      /* ------------------------------------- */

      /* 2. 第二次讀取: 讀取剩餘的 Payload
       * 錯誤修正 A: 存放位置要從 &pBuff[3] 開始 (接在 Header 後面)
       * 錯誤修正 B: 讀取長度應該是 payload_len (Header 說有多少就讀多少)
       */
      ret = i2c_read_dt(&pn7160_i2c, &pBuff[3], payload_len);

      if (ret == 0) {
        *pBytesRead = 3 + payload_len;  // 總長度 = Header(3) + Payload
      } else {
        *pBytesRead = 0;
      }
    } else {
      /* 只有 Header，沒有 Payload (例如某些回應只有 3 bytes) */
      *pBytesRead = 3;
    }
  } else {
    *pBytesRead = 0;
  }

  return ret;
}

static uint8_t tml_WaitForRx(uint16_t timeout) {
  if (timeout == 0) {
    int16_t to = 3000; /* 3 seconds */
    while (gpio_pin_get_dt(&pn7160_irq) == LOW) {
      k_msleep(1);
      to -= 1;
      if (to <= 0) {
        // printk("IRQ Timeout (3s)!\n");
        return 0xFF;
      }
    }
  } else {
    int16_t to = timeout;
    while ((gpio_pin_get_dt(&pn7160_irq) == LOW)) {
      k_msleep(1);
      to -= 1;
      if (to <= 0) {
        // printk("IRQ Timeout (%dms)!\n", timeout);
        return ERROR;
      }
    }
  }
  return SUCCESS;
}

void tml_Connect(void) {
  tml_Init();
  tml_Reset();
}

void tml_Disconnect(void) { return; }

void tml_Send(uint8_t* pBuffer, uint16_t BufferLen, uint16_t* pBytesSent) {
  if (tml_Tx(pBuffer, BufferLen) != SUCCESS) {
    *pBytesSent = 0;
  } else {
    *pBytesSent = BufferLen;
  }
}

void tml_Receive(uint8_t* pBuffer, uint16_t BufferLen, uint16_t* pBytes,
                 uint16_t timeout) {
  uint8_t ret = tml_WaitForRx(timeout);

  if (ret == ERROR)
    *pBytes = 0;
  else if (ret == 0xFF) {
    *pBytes = 0xFFFF;
  } else
    tml_Rx(pBuffer, BufferLen, pBytes);
}
