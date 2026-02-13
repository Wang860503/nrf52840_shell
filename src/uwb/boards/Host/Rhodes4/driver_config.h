/*
 * Copyright (c) 2013-2015, Freescale Semiconductor, Inc.
 * Copyright 2016-2017,2019,2022,2023 NXP
 *
 * Redistribution and use in source and binary forms, with or without
 modification,
 * are permitted provided that the following conditions are met:
 *
 * o Redistributions of source code must retain the above copyright notice, this
 list
 *   of conditions and the following disclaimer.
 *
 * o Redistributions in binary form must reproduce the above copyright notice,
 this
 *   list of conditions and the following disclaimer in the documentation and/or
 *   other materials provided with the distribution.

 * o Neither the name of Freescale Semiconductor, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __GPIO_PINS_H__
#define __GPIO_PINS_H__

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>

/*! @file */
/*!*/
/*! This file contains gpio pin definitions used by gpio peripheral driver.*/
/*! The enums in _gpio_pins map to the real gpio pin numbers defined in*/
/*! gpioPinLookupTable. And this might be different in different board.*/

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/* As per datasheet, taking to maximum 8Mhz of QN9090 */
#define UWB_SPI_BAUDRATE (8 * 1000 * 1000U)  // 8 MHz
#define HBCI_HEADER_SIZE 4
/* SPI */
#define UWB_SPI_NODE DT_ALIAS(uwbspi)

#define UWB_SPI_SSEL 0
#define ACCEL_SPI_SSEL 1

/* * [Zephyr GPIO Wrapper]
 * 定義一個結構來相容舊程式碼的 gpioInputPinConfig_t
 * 核心是包含 Zephyr 的 gpio_dt_spec。
 */
typedef struct {
  struct gpio_dt_spec spec;
  const char* label; /* 用於除錯或 Log */
} zephyr_pin_config_t;

/* 將舊型別映射到我們的新結構 */
typedef zephyr_pin_config_t gpioInputPinConfig_t;
typedef zephyr_pin_config_t gpioOutputPinConfig_t;

extern const gpioInputPinConfig_t dk6_button_io_pins[];
extern const gpioOutputPinConfig_t dk6_leds_io_pins[];

#define ledPins dk6_leds_io_pins
#define switchPins dk6_button_io_pins

#endif /* __GPIO_PINS_H__ */
