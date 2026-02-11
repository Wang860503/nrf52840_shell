#ifndef __em4095_H_

#define __em4095_H_

// #include "driver_config.h"
#include <nrfx_gpiote.h>
#include <nrfx_ppi.h>
#include <nrfx_timer.h>
#include <stdint.h>
#include <stdlib.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "buzzer.h"
// #define PORT_IRQ              PORT0
// #define PORT_VEN              PORT0
// #define PIN_IRQ               6 // P0.6 <- P1.010
// #define PIN_VEN               7 // P0.7 <- P1.08

#define PORT0 0
#define PORT1 1

#define PORT_EM4095_DEMOD PORT0
#define PORT_EM4095_SHD PORT1
#define PORT_EM4095_CLK PORT0
// #define PORT_EM4095_MOD       PORT0
#define PIN_EM4095_DEMOD 12
#define PIN_EM4095_SHD 9
#define PIN_EM4095_CLK 11
// #define PIN_EM4095_MOD        13

#define CALCULATE_SUCCESS_PERSENT 1

#define nrf_delay_ms(ms) k_msleep(ms)
#define nrf_delay_us(us) k_busy_wait(us)
#define NRF_LOG_INFO(...) \
  do {                    \
    printk(__VA_ARGS__);  \
    printk("\n");         \
  } while (0);

enum PPI_TRIGGER_MODE {
  FSK,
  ASK,
};

void em4095_receiver(void);
int em4095_gpio_init(void);
int em4095_enable(void);
void em4095_gpio_sampling_disable(void);

void em4095_shd_sleep(void);
void em4095_shd_emit_rf(void);
void em4095_set_trigger_mode(enum PPI_TRIGGER_MODE mode);

unsigned short em4095_clk_check(void);
unsigned short em4095_demod_check(void);

int em4095_timer3_init(void);
void em4095_timer3_deinit(void);

#endif /* #ifndef __em4095_H_ */