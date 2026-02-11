#include "buzzer.h"

void tone_gen(uint32_t freq, uint32_t duration) {
#ifdef BUZZER_SUPPORT
  /* 防呆：頻率不能為 0 */
  if (freq == 0) return;

  /* 2. 檢查裝置是否就緒 */
  if (!pwm_is_ready_dt(&buzzer)) {
    printk("Error: PWM device not ready\n");
    return;
  }

  /* 3. 計算週期 (Period) 與 脈寬 (Pulse) */
  /* Zephyr PWM API 單位通常是 奈秒 (nanoseconds) */
  /* 1秒 = 1,000,000,000 ns */
  uint32_t period_ns = 1000000000U / freq;

  /* 50% Duty Cycle: 脈寬 = 週期 / 2 */
  uint32_t pulse_ns = period_ns / 2;

  /* 4. 開始發聲 (Enable) */
  /* pwm_set_dt 會同時設定週期和脈寬，並啟動 PWM */
  int ret = pwm_set_dt(&buzzer, period_ns, pulse_ns);
  if (ret < 0) {
    printk("Error: Failed to set tone (err %d)\n", ret);
    return;
  }

  /* 5. 等待持續時間 */
  k_msleep(duration);

  /* 6. 停止發聲 (Disable) */
  /* 將脈寬設為 0 即可停止輸出 */
  pwm_set_dt(&buzzer, period_ns, 0);
#endif
}

void tone_powerup(void) {
  tone_gen(262 * 10, 200);
  tone_gen(330 * 10, 200);
  tone_gen(392 * 10, 200);
}
void tone_pn7150_detected(void) {
  tone_gen(392 * 10, 200);
  tone_gen(330 * 10, 200);
  tone_gen(262 * 10, 200);
}

void tone_pn7150_remove(void) {
  tone_gen(392 * 8, 200);
  tone_gen(392 * 8, 200);
  tone_gen(392 * 8, 200);
}
void tone_em4095_detected(void) { tone_gen(392 * 10, 50); }
void tone_em4095_failed(void) {}
void tone_em4095_demod_failed(void) {}
