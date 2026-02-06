#ifndef __buzzer_H_

#define __buzzer_H_

#include <stdbool.h>
#include <stdint.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>
#include <zephyr/pm/device.h>
#include <zephyr/pm/policy.h>

static const struct pwm_dt_spec buzzer = PWM_DT_SPEC_GET(DT_ALIAS(pwmbuzzer));

static volatile bool ready_flag;

void tone_gen(uint32_t freq, uint32_t duration);
void tone_powerup(void);
void tone_pn7150_detected(void);
void tone_em4095_detected(void);
void tone_em4095_failed(void);
void tone_em4095_demod_failed(void);

#endif /* #ifndef __pwm_H_ */