/*
 * Project name
     RFiD (Displaying CRC check of RFid card via Usart)
 * Copyright
     (c) mikroElektronika, 2010.
 * Revision History
     20091220:
       - initial release;
 * Description
     The code demonstrates using two external interrupts to read data sent
     by EM4095 chip (clock - RDY/CLK; data - OUT).
     Upon correct identification of the card, results are displayed via USART
     along with the card specific number.
 * Test configuration:
     MCU:             PIC18F4520
                      http://ww1.microchip.com/downloads/en/DeviceDoc/39631E.pdf
     Dev.Board:       EasyPIC6
                      http://www.mikroe.com/eng/products/view/297/easypic6-development-system/
     Oscillator:      HS-PLL, 32.0000 MHz
     Ext. Modules:    mE RFid Reader Board
                      ac:RFid_reader
                      http://www.mikroe.com/eng/products/view/185/rfid-reader-board/
     SW:              mikroC PRO for dsPIC30/33 and PIC24
                      http://www.mikroe.com/eng/products/view/231/mikroc-pro-for-dspic30-33-and-pic24/
 * NOTES:
     - mE RFid Reader Board should be connected to PORTA
     - Make sure you turn on the apropriate switches to enable USART
 communication (board specific)
     - Upon correct CRC check program will send "CRC CHECK OK!" via USART
     -  Connections can be established using flexible wire jumpers:
 http://www.mikroe.com/eng/products/view/495/wire-jumpers/ Used pins: Vcc, Gnd
                RA14 - OUT
                RA15 - RDY/CLK
                RA9 - SHD
                RA10 - MOD
*/
#include "em4095.h"

K_SEM_DEFINE(em4095_sem, 1, 1);

unsigned short sync_flag,  // in the sync routine if this flag is set
    one_seq,               // counts the number of 'logic one' in series
    data_in,     // gets data bit depending on data_in_1st and data_in_2nd
    cnt,         // interrupt counter
    cnt1, cnt2,  // auxiliary counters
    loopflag;    // indictae in loop or not
unsigned short detect_bus_counter;
unsigned short data_index;  // marks position in data arrey
char i;
char _data[256];
char data_valid[64];
char bad_synch;  // variable for detecting bad synchronization
unsigned short em_digit = 0;
unsigned long long em_card_code = 0;
unsigned short volatile em4095_detected;
unsigned short volatile em4095_reboot = 0;

const struct gpio_dt_spec demod_out_gpio =
    GPIO_DT_SPEC_GET(DT_ALIAS(em4095demodout), gpios);
const struct gpio_dt_spec shd_gpio =
    GPIO_DT_SPEC_GET(DT_ALIAS(em4095shd), gpios);
const struct gpio_dt_spec clk_gpio =
    GPIO_DT_SPEC_GET(DT_ALIAS(em4095clk), gpios);
/* undef for modiified hardware */
#define EM4095_SHD_HW_MOD 1 /* due to VIH needs 4V */

/* modified hw for BV test */
#define EM4095_SHD_WORKAROUND 0

#define EM4095_FSK_DETECTION 1

#ifdef EM4095_FSK_DETECTION
/*
#include "nrf.h"
#include "nrf_drv_gpiote.h"
#include "nrf_drv_timer.h"
// #include "bf_gpio.h"
#include "app_error.h"
*/
static struct gpio_callback demod_cb_data;
static uint16_t demod_counter = 0;
static uint16_t demod_check_counter = 0;
static uint16_t em4095_timer_expired = 0;

#define TICK_BUFFER_SIZE ((96 * 2 + 8) * 6) /* 200 */
uint32_t tick_buffer[TICK_BUFFER_SIZE] = {0, 0};
uint8_t fsk_bit_buffer[TICK_BUFFER_SIZE] = {0, 0};

/* 原本是 0x480 (1152) */
/* FSK '0' 約 975 ticks, FSK '1' 約 1220 ticks */
/* 中間值約 1097, 設定為 1100 以獲得最佳平衡 */
#define MIN_FSK_1_TICK 0x44C /* 1/12.5KHz = 80us, 1280 ticks when 16MHz */

uint32_t preview_tick = 0;
uint32_t current_tick = 0;
uint32_t threadhold = 0;
uint8_t fsk_bit_0_counter = 0;
uint8_t fsk_bit_1_counter = 0;
uint16_t header_position = 0;
uint8_t data_bit_count = 0;
uint8_t data_buf[96 - 6];
uint32_t original_data[3] = {0, 0};
uint8_t fsk_card_detected = 1;
uint8_t real_44bits_buffer[44];
uint16_t company_code, card_format, sc_code, ep, op, ep_sum, op_sum;
uint32_t pre_card_number = 0;
uint32_t card_number = 0;
uint16_t ep_35bit_sum, op_35bit_sum_all, op_35bit_sum;
uint16_t ep_37bit_sum, op_37bit_sum;
uint16_t ep_37bit_10302_sum, op_37bit_10302_sum;
uint16_t op1_sum, ep2_sum, op3_sum;
const nrfx_timer_t TIMER_EM4095 = NRFX_TIMER_INSTANCE(3);

static uint32_t last_capture_tick = 0;
static nrf_ppi_channel_t m_ppi_channel;
static const nrfx_gpiote_t m_gpiote = NRFX_GPIOTE_INSTANCE(0);
static bool m_gpiote_initialized = false;
#define EM4095_CAPTURE_CC_IDX 0
#define NOISE_GLITCH_THRESHOLD 200

void em4095_set_trigger_mode(enum PPI_TRIGGER_MODE mode) {
  uint8_t ch_index;
  nrfx_err_t err_code =
      nrfx_gpiote_channel_get(&m_gpiote, demod_out_gpio.pin, &ch_index);
  if (err_code != NRFX_SUCCESS) return;

  /* 先停用以進行修改 */
  nrf_gpiote_event_disable(NRF_GPIOTE, ch_index);
  nrf_gpiote_int_disable(NRF_GPIOTE, 1UL << ch_index);

  if (mode == ASK) {
    /* EM Mode: 雙邊緣觸發 (Toggle) 以捕捉脈衝寬度 */
    nrf_gpiote_event_configure(NRF_GPIOTE, ch_index, demod_out_gpio.pin,
                               NRF_GPIOTE_POLARITY_TOGGLE);
  } else {
    /* FSK Mode: 上升緣觸發 (LoToHi) */
    nrf_gpiote_event_configure(NRF_GPIOTE, ch_index, demod_out_gpio.pin,
                               NRF_GPIOTE_POLARITY_LOTOHI);
  }

  /* 重新啟用 */
  nrf_gpiote_event_enable(NRF_GPIOTE, ch_index);
  nrf_gpiote_int_enable(NRF_GPIOTE, 1UL << ch_index);
}

/* compare timer to calculate duration between low to high */
void pin_demod_handler(const struct device* dev, struct gpio_callback* cb,
                       uint32_t pins) {
  // nrf_drv_gpiote_out_toggle(PIN_OUT);
  if (pins & BIT(PIN_EM4095_DEMOD)) {
    current_tick = nrfx_timer_capture(&TIMER_EM4095, NRF_TIMER_CC_CHANNEL3);
    tick_buffer[demod_counter] = current_tick - preview_tick;
    preview_tick = current_tick;
    demod_counter++;
    demod_check_counter++;
    if (demod_counter >= TICK_BUFFER_SIZE) {
      // NRF_LOG_INFO("DEMOD_INTERRUPT!(%d) (%d)\n", tick_buffer[49],
      // tick_buffer[50]);
      em4095_gpio_sampling_disable();
      // demod_counter = 0;
    }
#if 0
        if((demod_counter & 1) == 1)
        {
            //NRF_LOG_INFO("DEMOD_INTERRUPT!(%d)\n", demod_counter);
            gpio_SetValue(1, 8, HIGH);
        }
        else
        {
            gpio_SetValue(1, 8, LOW);
        }
#endif
    // nrf_drv_timer_clear(&TIMER_EM4095);
  }
}

void demod_gpio_handler(nrfx_gpiote_pin_t pin, nrfx_gpiote_trigger_t trigger,
                        void* p_context) {
  if (demod_counter < TICK_BUFFER_SIZE) {
    // 1. 讀取 PPI 剛剛鎖存的「絕對時間」
    // 直接讀取暫存器，因為這個值是硬體在 event 發生的瞬間拍下的
    uint32_t current_absolute_tick = nrfx_timer_capture_get(
        &TIMER_EM4095, (nrf_timer_cc_channel_t)EM4095_CAPTURE_CC_IDX);

    // 2. 軟體相減計算 Delta (模擬原程式碼: tick = current - preview)
    // 這裡維持「累積計時」邏輯，不歸零 Timer，容錯率最高
    uint32_t period = current_absolute_tick - last_capture_tick;

    tick_buffer[demod_counter] = period;

    // 3. 更新 last_capture_tick
    last_capture_tick = current_absolute_tick;

    demod_counter++;
    demod_check_counter++;

    if (demod_counter >= TICK_BUFFER_SIZE) {
      em4095_gpio_sampling_disable();
    }
  }
}

void em4095_timer_event_handle(nrf_timer_event_t event_type, void* p_context) {
  /* NULL */
  // NRF_LOG_INFO("TIMER INTERRUPT!(%d) (%d)\n", tick_buffer[49],
  // tick_buffer[50]);
  switch (event_type) {
    case NRF_TIMER_EVENT_COMPARE3:
      em4095_timer_expired = 1;
      nrfx_timer_disable(&TIMER_EM4095);
      nrfx_timer_clear(&TIMER_EM4095);

      em4095_gpio_sampling_disable();
      if (demod_counter < (TICK_BUFFER_SIZE / 2)) {
        fsk_card_detected = 0;
      }
      // NRF_LOG_INFO("em4095 timer3 isr!!");
      break;

    default:
      // Do nothing.
      break;
  }
}

void em4095_timer_enable(void) {
  /* start timer */
  uint32_t time_ms =
      84;  // Time(in miliseconds) between consecutive compare events.
  uint32_t time_ticks;
  em4095_timer_expired = 0; /* clear */
  // demod_counter = 0;

  time_ticks = nrfx_timer_ms_to_ticks(&TIMER_EM4095, time_ms);

  nrfx_timer_extended_compare(&TIMER_EM4095, NRF_TIMER_CC_CHANNEL3, time_ticks,
                              NRF_TIMER_SHORT_COMPARE3_CLEAR_MASK, true);

  nrfx_timer_enable(&TIMER_EM4095);
  // preview_tick = nrf_drv_timer_capture(&TIMER_EM4095, NRF_TIMER_CC_CHANNEL3);
}

void em4095_timer_disable(void) {
  /* stop timer */
  nrfx_timer_disable(&TIMER_EM4095);
}

void em4095_gpio_sampling_enable(void) {
  last_capture_tick = 0;
  nrfx_timer_clear(&TIMER_EM4095);

  /* v3: 啟用 Trigger (包含中斷與 Event) */
  nrfx_gpiote_trigger_enable(&m_gpiote, demod_out_gpio.pin, true);

  /* 重要：v3 需要手動 hook 你的 callback 到 GPIOTE IRQ。
     如果你發現編譯無法通過 input_configure 設定 handler，
     你可能需要直接使用 nrfx_gpiote_global_callback_set(&demod_gpio_handler,
     NULL);
  */

  nrfx_ppi_channel_enable(m_ppi_channel);
  // preview_tick = nrf_drv_timer_capture(&TIMER_EM4095, NRF_TIMER_CC_CHANNEL3);
}

void em4095_gpio_sampling_disable(void) {
  nrfx_ppi_channel_disable(m_ppi_channel);
  /* v3: 停用 Trigger */
  nrfx_gpiote_trigger_disable(&m_gpiote, demod_out_gpio.pin);
}

int em4095_timer3_init(void) {
  nrfx_err_t err_code = NRFX_SUCCESS;

  /* 1. Timer Init */
  nrfx_timer_config_t timer_cfg =
      NRFX_TIMER_DEFAULT_CONFIG(NRF_TIMER_BASE_FREQUENCY_16MHZ);
  timer_cfg.bit_width = NRF_TIMER_BIT_WIDTH_32;
  err_code =
      nrfx_timer_init(&TIMER_EM4095, &timer_cfg, em4095_timer_event_handle);
  if (err_code != NRFX_SUCCESS) {
    printk("Error: nrfx_timer_init fail code: %d\n", err_code);
    return -EIO;
  }

  /* 2. PPI Alloc */
  err_code = nrfx_ppi_channel_alloc(&m_ppi_channel);
  if (err_code != NRFX_SUCCESS) {
    printk("Error: PPI alloc failed\n");
    return -EIO;
  }

  /* 3. GPIOTE Init (Handle Already Initialized) */
  if (!m_gpiote_initialized) {
    err_code = nrfx_gpiote_init(&m_gpiote, 1);
    if (err_code == NRFX_SUCCESS ||
        err_code == NRFX_ERROR_ALREADY_INITIALIZED) {
      m_gpiote_initialized = true;
    } else {
      printk("Error: GPIOTE init failed: %d\n", err_code);
    }
  }

  /* 4. Channel Alloc */
  uint8_t ch_index;
  err_code = nrfx_gpiote_channel_alloc(&m_gpiote, &ch_index);
  if (err_code != NRFX_SUCCESS) {
    printk("Error: GPIOTE Channel Alloc Failed! Code: %d\n", err_code);
    return -EIO;
  }

  /* 5. 【關鍵修正】先 Uninit Pin，清除 Zephyr 狀態 */
  /* 如果這個 Pin 已經被 Zephyr 使用，nrfx 內部狀態可能是不一致的，強制重置它 */
  nrfx_gpiote_pin_uninit(&m_gpiote, demod_out_gpio.pin);

  /* 6. 設定結構體 */
  static const nrf_gpio_pin_pull_t pull_config = NRF_GPIO_PIN_NOPULL;

  nrfx_gpiote_trigger_config_t trigger_config = {
      .trigger = NRFX_GPIOTE_TRIGGER_LOTOHI,
      .p_in_channel = &ch_index,
  };

  nrfx_gpiote_handler_config_t handler_config = {
      .handler = (nrfx_gpiote_interrupt_handler_t)demod_gpio_handler,
      .p_context = NULL,
  };

  nrfx_gpiote_input_pin_config_t input_config = {
      .p_pull_config = &pull_config,
      .p_trigger_config = &trigger_config,
      .p_handler_config = &handler_config,
  };

  /* 7. 套用設定 */
  err_code =
      nrfx_gpiote_input_configure(&m_gpiote, demod_out_gpio.pin, &input_config);
  if (err_code != NRFX_SUCCESS) {
    printk("Error: Input Configure failed: 0x%08X\n", err_code);
    return -EIO;
  }

  /* 8. 【關鍵修正】改用 HAL 直接啟用 Trigger */
  /* * 這裡不呼叫 nrfx_gpiote_trigger_enable，因為它會檢查 pin_has_trigger 導致
   * Assert。 我們直接操作硬體，確保 Event 和 Interrupt 開啟。
   */

  /* 確保 Event 模式與極性正確 (冗餘但安全) */
  nrf_gpiote_event_configure(NRF_GPIOTE, ch_index, demod_out_gpio.pin,
                             NRF_GPIOTE_POLARITY_LOTOHI);

  /* 開啟 Event (給 PPI 用) */
  nrf_gpiote_event_enable(NRF_GPIOTE, ch_index);

  /* 開啟中斷 (給 CPU 用) */
  nrf_gpiote_int_enable(NRF_GPIOTE, 1UL << ch_index);

  /* 9. Event Address for PPI */
  nrf_gpiote_event_t gpiote_event = nrf_gpiote_in_event_get(ch_index);
  uint32_t gpiote_event_addr =
      nrf_gpiote_event_address_get(NRF_GPIOTE, gpiote_event);

  /* 10. PPI Assignment */
  uint32_t timer_capture_task_addr =
      nrfx_timer_capture_task_address_get(&TIMER_EM4095, EM4095_CAPTURE_CC_IDX);
  nrfx_ppi_channel_assign(m_ppi_channel, gpiote_event_addr,
                          timer_capture_task_addr);

  /* 11. IRQ Connect */
  IRQ_CONNECT(TIMER3_IRQn, 1, nrfx_isr, nrfx_timer_3_irq_handler, 0);
  irq_enable(TIMER3_IRQn);

  printk("EM4095 Timer3 & PPI Initialized Success\n");
  return 0;
}

void em4095_timer3_deinit(void) {
  /* Disable GPIO sampling first (this also disables PPI channel) */
  em4095_gpio_sampling_disable();

  /* Disable timer */
  em4095_timer_disable();

  /* Disable timer interrupts before uninit */
  irq_disable(TIMER3_IRQn);

  /* Uninitialize timer to release it */
  nrfx_timer_uninit(&TIMER_EM4095);

  /* Free PPI channel */
  nrfx_ppi_channel_free(m_ppi_channel);
}

unsigned short em4095_hid_receiver(void) {
  /* 1. confirm FSK signal
   * 2. open detection windows to detect 0x1D(detect 18 times 15.625KHz square
   * wave) 1 bit 400us, needs (96+8) * 400 = 41.60ms should be detect one time
   * at least in 42 ms if get 0x1D, should be detect one time to receive all
   * data again.
   * 3. timer interrupt service to detect fsk 0 and fsk 1
   *    fsk 0 : 15.625KHz * 6 (64us * 6 = 384us) -> 63.36-64.64 us
   *    fsk 1 : 12.5KHz * 5 (80us * 5 = 400us) -> 79.2-80.8 us
   *
   */
  uint16_t i;
  uint32_t redundant_word_26 = 0, redundant_word_35 = 0, redundant_word_37 = 0;
  unsigned short ret = 0;

  /* 42 ms timer test */
  /* need to find 0x1D(0x0001 1101), means 18*fsk_0 and 15*fsk_1 at least */
  memset(tick_buffer, 0, TICK_BUFFER_SIZE);
  memset(fsk_bit_buffer, 0, TICK_BUFFER_SIZE);
  while (1) {
    fsk_card_detected = 1;
    em4095_timer_enable();
    em4095_timer_expired = 0;
    demod_counter = 0;
    em4095_gpio_sampling_enable();
    // gpio_SetValue(1, 8, HIGH);
    // while(!em4095_timer_expired)
    //{
    nrf_delay_ms(84);
    //}
    // gpio_SetValue(1, 8, LOW);
    em4095_gpio_sampling_disable();
    em4095_timer_disable();
    if (demod_counter < (TICK_BUFFER_SIZE / 2)) {
      fsk_card_detected = 0;
    }
    break;
  }
  // NRF_LOG_INFO("fsk_card_detected = %d\n", fsk_card_detected);
  if (fsk_card_detected) {
#if 0
        /* decision adjustment */
        //NRF_LOG_INFO("[em4095] HID DETECTED!!!"); 
        uint16_t queue_data_1[64], queue_data_2[22];
        uint16_t queue_1_index = 0, queue_2_index = 0;
        
        for(i = 10; i < TICK_BUFFER_SIZE; i++)
        {
            if((tick_buffer[i] > 800) && (tick_buffer[i] < 1500))
            {
                queue_data_1[queue_1_index++] = tick_buffer[i];
                if(queue_1_index >= 64)
                {
                    break;
                }
            }
        }
        threadhold = 0;
        for(i = 0; i < 64; i++)
        {
            threadhold += queue_data_1[i];
        }
        threadhold >>= 6;
        uint8_t pre_bit = 0;
        queue_1_index = 0;
        for(i = 1; i < TICK_BUFFER_SIZE; i++)
        {
            if(tick_buffer[i] > 1500)
            {
                if(pre_bit)
                {
                    fsk_bit_buffer[queue_1_index++] = 1;
                    fsk_bit_buffer[queue_1_index++] = 0;
                }
                else
                {
                    fsk_bit_buffer[queue_1_index++] = 0;
                    fsk_bit_buffer[queue_1_index++] = 1;                    
                }
            }
            else if(tick_buffer[i] > threadhold)
            {
                fsk_bit_buffer[queue_1_index++] = 1;
                pre_bit = 1;
                //queue_1_index++;
            }

            else
            {
                fsk_bit_buffer[queue_1_index++] = 0;
                pre_bit = 0;
                //queue_1_index++;
            }
            if(queue_1_index>=TICK_BUFFER_SIZE)
            {
                break;
            }
        }
#else
    /*
    NRF_LOG_INFO("--- HID RAW TICKS ---");
    for (int k = 0; k < 20; k += 2) {
      // 印出完整週期 (Low + High)
      uint32_t period = tick_buffer[k] + tick_buffer[k + 1];
      NRF_LOG_INFO("[%d] %u", k, period);
    }
    */

    uint32_t accumulated_tick = 0;
    for (i = 0; i < TICK_BUFFER_SIZE; i++) {
      /* 取出當前數值 */
      uint32_t current_val = tick_buffer[i];

      /* 如果有累積的數值 (之前有雜訊)，加進來 */
      current_val += accumulated_tick;
      accumulated_tick = 0;  // 清空累積

      /* 檢查是否為極短雜訊 (Glitch) */
      if (current_val < NOISE_GLITCH_THRESHOLD) {
        /* 這是一個雜訊！不要判斷它，把它加到下一個數值去修復波形 */
        accumulated_tick = current_val;

        /* 暫時將這個 bit 標記為無效或預設值 (之後判斷 header 會自動忽略) */
        fsk_bit_buffer[i] = 0;
        continue;
      }

      if (tick_buffer[i] > MIN_FSK_1_TICK) {
        fsk_bit_buffer[i] = 1;
      } else
        fsk_bit_buffer[i] = 0;
    }
#endif
    /* search header */
    // NRF_LOG_INFO("[em4095] search header...");
    fsk_bit_0_counter = 0;
    fsk_bit_1_counter = 1;
    header_position = 0;
    memset(data_buf, 0xFF, sizeof(data_buf));
    for (i = 0; i < TICK_BUFFER_SIZE; i++) {
      if (fsk_bit_buffer[i] == 0) {
        fsk_bit_0_counter++;
      } else {
        if (fsk_bit_0_counter >= 14) {
          header_position = i; /* find the position of first fsk bit 1 */
          break;
        } else {
          fsk_bit_0_counter = 0;
        }
      }
    }
    if (header_position) {
      for (i = header_position; i < TICK_BUFFER_SIZE; i++) {
        if (fsk_bit_buffer[i] == 1) {
          fsk_bit_1_counter++;
        } else {
          if (fsk_bit_1_counter >= 13) {
            header_position =
                i; /* find the position of the last 2 bit of header */
            break;
          } else {
            fsk_bit_1_counter = 0;
          }
        }
      }
      data_bit_count = 0;
      fsk_bit_0_counter = fsk_bit_1_counter = 0;
      /* decision the fsk bit to data bit */
      for (i = header_position; i < TICK_BUFFER_SIZE; i++) {
        if (fsk_bit_buffer[i] == 1) {
          fsk_bit_1_counter++;
          if (fsk_bit_0_counter >= 4) {
            if (fsk_bit_0_counter > 8) {
              data_buf[data_bit_count++] = 0;
            }
            data_buf[data_bit_count++] = 0;
            fsk_bit_0_counter = 0;
          }
        } else {
          fsk_bit_0_counter++;
          if (fsk_bit_1_counter >= 4) {
            if (fsk_bit_1_counter > 8) {
              data_buf[data_bit_count++] = 1;
            }
            data_buf[data_bit_count++] = 1;
            fsk_bit_1_counter = 0;
          }
        }
        if (data_bit_count >= 90) {
          /* all data received */
          break;
        }
      }
#if 0
     for(i = 0; i < 26; i++)
     {
        original_data[0] |= (data_buf[i] << (25-i));
     }
     for(i = 26; i < 58; i++)
     {
        original_data[1] |= (data_buf[i] << (57-i));
     }
     for(i = 58; i < 90; i++)
     {
        original_data[2] |= (data_buf[i] << (89-i));
     }
     NRF_LOG_INFO("[em4095] HID card was detected!(0x%8X)(0x%8X)(0x%8X)", original_data[0], original_data[1], original_data[2]);
#endif
      memset(real_44bits_buffer, 0xFF, sizeof(real_44bits_buffer));
      for (i = 0; i < 88; i += 2) {
        real_44bits_buffer[43 - i / 2] = i + 1;
        if ((data_buf[2 + i] == 0) && (data_buf[2 + i + 1] == 1)) {
          real_44bits_buffer[43 - i / 2] = 0;
        } else if ((data_buf[2 + i] == 1) && (data_buf[2 + i + 1] == 0)) {
          real_44bits_buffer[43 - i / 2] = 1;
        } else {
          /* should not happen break! */
          // printk("should not happen break!\n");
          return 0;
        }
      }

      /* parity check */
      // NRF_LOG_INFO("[em4095] parity check...");
      /* HID 10302 */
      ep_sum = 0;
      op_sum = 0;
      /* should be 0 if even parity check */
      for (i = 13; i < 26; i++) ep_sum += (real_44bits_buffer[i] & 0x01);
      /* should be 1 if odd parity check */
      for (i = 0; i < 13; i++) op_sum += (real_44bits_buffer[i] & 0x01);
      /* 18 bits, 0x00801 */
      for (i = 26; i < 44; i++) {
        redundant_word_26 |= (real_44bits_buffer[i] << (i - 26));
      }
      /* corp 1000 35bits */ /* 34-0 */
      ep_35bit_sum = op_35bit_sum_all = op_35bit_sum = 0;
      ep_35bit_sum = real_44bits_buffer[33];
      op_35bit_sum = real_44bits_buffer[0];
      for (i = 1; i < 34; i += 3) {
        ep_35bit_sum += (real_44bits_buffer[i] + real_44bits_buffer[i + 1]);
        op_35bit_sum += (real_44bits_buffer[i + 1] + real_44bits_buffer[i + 2]);
      }
      for (i = 0; i < 35; i += 1) {
        op_35bit_sum_all += real_44bits_buffer[0];
      }
      /* 9 bits, 0x005 */
      for (i = 35; i < 44; i++) {
        redundant_word_35 |= (real_44bits_buffer[i] << (i - 35));
      }
      /* HID H10304 or H10302 */
      ep_37bit_sum = 0;
      op_37bit_sum = 0;
      for (i = 18; i < 37; i++) ep_37bit_sum += (real_44bits_buffer[i] & 0x01);
      for (i = 0; i < 19; i++) op_37bit_sum += (real_44bits_buffer[i] & 0x01);
      /* 7 bits, 0x00 */
      for (i = 37; i < 44; i++) {
        redundant_word_37 |= (real_44bits_buffer[i] << (i - 35));
      }

      /* show result */
      pre_card_number = card_number;
      card_number = 0;
      // if(((ep_sum & 0x01) == 0) && ((op_sum & 0x01) == 1) &&
      // (real_44bits_buffer[25] || real_44bits_buffer[26]) &&
      // (real_44bits_buffer[27] == 0))
      NRF_LOG_INFO("[em4095] show result...");
      if (((ep_sum & 0x01) == 0) && ((op_sum & 0x01) == 1) &&
          (redundant_word_26 == 0x0801)) {
        for (i = 1; i < 17; i++)
          card_number |= (real_44bits_buffer[i] << (i - 1));
        if (card_number != pre_card_number) {
          NRF_LOG_INFO("[em4095] HID(26-bits) Card NO: (%d)", card_number);
        } else {
          NRF_LOG_INFO("[em4095] [duplicate] HID(26 bits) Card NO: (%d)",
                       card_number);
        }
        tone_em4095_detected();
        ret = 1;
      }
      // else if(((ep_35bit_sum & 0x01) == 0) && ((op_35bit_sum & 0x01) == 1) &&
      // ((op_35bit_sum_all & 0x01) == 1))
      else if (((ep_35bit_sum & 0x01) == 0) && ((op_35bit_sum & 0x01) == 1) &&
               ((op_35bit_sum_all & 0x01) == 1) &&
               (redundant_word_35 == 0x005)) {
        card_number = 0;
        for (i = 1; i < 21; i++)
          card_number |= (real_44bits_buffer[i] << (i - 1));
        /* with facility code, H10304 */
        if (card_number != pre_card_number) {
          NRF_LOG_INFO("[em4095] HID(35 bits) Card NO: (%d)", card_number);
        } else {
          NRF_LOG_INFO("[em4095] [duplicate] HID(35 bits) Card NO: (%d)",
                       card_number);
        }
        ret = 1;
      }
      // else if(((ep_37bit_sum & 0x01) == 0) && ((op_37bit_sum & 0x01) == 1) &&
      // (real_44bits_buffer[36] || real_44bits_buffer[37]) &&
      // (real_44bits_buffer[38] == 0))
      else if (((ep_37bit_sum & 0x01) == 0) && ((op_37bit_sum & 0x01) == 1) &&
               (redundant_word_37 == 0x00)) {
        card_number = 0;
        for (i = 1; i < 20; i++)
          card_number |= (real_44bits_buffer[i] << (i - 1));
        /* with facility code, H10304 */
        if (card_number != pre_card_number) {
          NRF_LOG_INFO("[em4095] HID(37 bits) Card NO: (%d)", card_number);
        } else {
          NRF_LOG_INFO("[em4095] [duplicate] HID(37 bits) Card NO: (%d)",
                       card_number);
        }
        ret = 1;
      } else {
        NRF_LOG_INFO("[em4095] HID CRC failed!)");
      }
    }
#if 0
    /* removed it due to show this message when no card nearby */
    else
    {

        NRF_LOG_INFO("[em4095] failed to find heeader (%d)", threadhold);
    }
#endif
  }
  return ret;
}

#endif /* #ifdef EM4095_FSK_DETECTION */

int em4095_gpio_init(void) {
  int ret;
  nrfx_err_t err_code = NRFX_SUCCESS;

  if (!gpio_is_ready_dt(&demod_out_gpio) || !gpio_is_ready_dt(&shd_gpio) ||
      !gpio_is_ready_dt(&clk_gpio)) {
    printk("Error: EM4095 GPIOs not ready\n");
    return -EIO;
  }

#ifdef EM4095_FSK_DETECTION

#else  /* #ifdef EM4095_FSK_DETECTION */
  gpio_pin_configure_dt(&demod_out_gpio, GPIO_INPUT);
#endif /* #ifdef EM4095_FSK_DETECTION */
  /* Configure GPIO for em4095 */
  gpio_pin_configure_dt(&clk_gpio, GPIO_INPUT);
  // gpio_SetDir(PORT_EM4095_MOD, PIN_EM4095_MOD, SET_OUT);

#ifdef EM4095_SHD_WORKAROUND
  /* hardware used pull-up resitor */
  /* set gpio to input ping let SHD is high */
  gpio_pin_configure_dt(&shd_gpio, GPIO_INPUT);
#else                    /* #ifdef EM4095_SHD_WORKAROUND */

  /* due to SHD voltage is not enough, we should be modify hardware to fix it */
#ifdef EM4095_SHD_HW_MOD /* original without modifying hardware */
  gpio_pin_configure_dt(&shd_gpio, GPIO_OUTPUT_ACTIVE);
#else
  gpio_pin_configure_dt(&shd_gpio, GPIO_OUTPUT_INACTIVE);
#endif

#endif /* #ifdef EM4095_SHD_WORKAROUND */
  printk("EM4095 GPIO Initialized\n");
  return 0;
}

static void em4095_reset(void) {
#ifdef EM4095_SHD_WORKAROUND
  em4095_shd_sleep();
  nrf_delay_ms(1);
  em4095_shd_emit_rf();
#else                    /* #ifdef EM4095_SHD_WORKAROUND */
#ifdef EM4095_SHD_HW_MOD /* original without modifying hardware */
  gpio_pin_configure_dt(&shd_gpio, GPIO_OUTPUT_ACTIVE);
  nrf_delay_ms(1);
  gpio_pin_configure_dt(&shd_gpio, GPIO_OUTPUT_INACTIVE);
#else                    /* #ifdef EM4095_SHD_HW_MOD */
  gpio_SetValue(PORT_EM4095_SHD, PIN_EM4095_SHD, LOW);
  nrf_delay_ms(1);
  gpio_SetValue(PORT_EM4095_SHD, PIN_EM4095_SHD, HIGH);
#endif                   /* #ifdef EM4095_SHD_HW_MOD */
#endif                   /* #ifdef EM4095_SHD_WORKAROUND */
}

/* SHD:
   high - sleep
   low - enable
 */
void em4095_shd_sleep(void) {
#ifdef EM4095_SHD_WORKAROUND
  /* set gpio to input ping let SHD is high */
  gpio_pin_configure_dt(&shd_gpio, GPIO_OUTPUT_ACTIVE);
#else /* #ifdef EM4095_SHD_WORKAROUND */

#ifdef EM4095_SHD_HW_MOD /* original without modifying hardware */
  gpio_pin_configure_dt(&shd_gpio, GPIO_OUTPUT_INACTIVE);
#else
  gpio_pin_configure_dt(&shd_gpio, GPIO_OUTPUT_ACTIVE);
#endif

#endif /* #ifdef EM4095_SHD_WORKAROUND */
}

void em4095_shd_emit_rf(void) {
#ifdef EM4095_SHD_WORKAROUND
  gpio_pin_configure_dt(&shd_gpio, GPIO_OUTPUT_INACTIVE);
#else /* #ifdef EM4095_SHD_WORKAROUND */

#ifdef EM4095_SHD_HW_MOD /* original without modifying hardware */
  gpio_pin_configure_dt(&shd_gpio, GPIO_OUTPUT_ACTIVE); /* sleep mod*/
#else
  gpio_pin_configure_dt(&shd_gpio, GPIO_OUTPUT_INACTIVE);
#endif

#endif /* #ifdef EM4095_SHD_WORKAROUND */
}

int em4095_enable(void) {
#ifdef EM4095_FSK_DETECTION
  // NRF_LOG_INFO("em4095 em4095_timer3_init!!");
  int err = em4095_timer3_init();
  if (err != 0) {
    return err;
  }
#endif /* #ifdef EM4095_FSK_DETECTION */
  em4095_shd_emit_rf();
  return 0;
}

char CRC_Check(char* bit_array) {
  char row_count, row_bit, column_count;
  char row_sum, column_sum;
  char row_check[5];
  char column_check[11];

  // row parity check:
  row_count = 9;  // count rows
  while (row_count < 59) {
    column_count = 0;  // count columns
    while (column_count < 5) {
      row_check[column_count] = bit_array[row_count + column_count];
      column_count++;
    }
    row_bit = 0;  // count row bits
    row_sum = 0;
    while (row_bit < 4) {
      row_sum = row_sum + row_check[row_bit];
      row_bit++;
    }

    // if (row_sum.B0 != row_check[4].B0)
    if ((row_sum & 0x01) != (row_check[4] & 0x01)) {
      return 0;
    }
    row_count = row_count + 5;
  }
  // end row parity check

  // column parity check
  column_count = 9;  // count columns
  while (column_count < 13) {
    row_bit = 0;    // count column bits
    row_count = 0;  // count rows
    while (row_bit < 11) {
      column_check[row_bit] = bit_array[column_count + row_count];
      row_bit++;
      row_count = row_count + 5;
    }

    row_bit = 0;  // count column bits
    column_sum = 0;
    while (row_bit < 10) {
      column_sum = column_sum + column_check[row_bit];
      row_bit++;
    }

    // if (column_sum.B0 != column_check[10].B0)
    if ((column_sum & 0x01) != (column_check[10] & 0x01)) {
      return 0;
    }
    column_count++;
  }
  // end column parity check
  if (bit_array[63] == 1) {
    return 0;
  }
  return 1;
}

unsigned short em4095_demod_check(void) {
  unsigned short result = 0, demod_check_timeout = 0, preStatus = 0;
  demod_check_counter = 0;
  em4095_reset();
  nrf_delay_ms(30);
  preStatus = gpio_pin_get_dt(&demod_out_gpio);
  ;
  while (demod_check_timeout < 20000) {
    demod_check_timeout++;
    nrf_delay_us(50);
    if (preStatus != gpio_pin_get_dt(&demod_out_gpio)) {
      preStatus = gpio_pin_get_dt(&demod_out_gpio);
      demod_check_counter++;
    }
    if (demod_check_counter >= 2) {
      result = 1;
      break;
    }
  }
  em4095_shd_sleep();
  return result; /* failed if 1 */
}

unsigned short em4095_clk_check(void) {
  /* 125KHz --> 8us per cycle */
  /* use to check 5V control work or not */
  unsigned short result = 1, detect_counter = 0;
  gpio_pin_configure_dt(&clk_gpio, GPIO_INPUT);
  /* do reset */
  nrf_delay_ms(10);
  em4095_reset();
  /* needs 25.60ms delay at least */
  nrf_delay_ms(30);

  while (gpio_pin_get_dt(&clk_gpio) == 1) {
    nrf_delay_us(2);
    detect_counter++;
    if (detect_counter >= 6) {
      result = 0;    /* clk did not work */
      return result; /* escape while loop */
    }
  }
  detect_counter = 0;
  while (gpio_pin_get_dt(&clk_gpio) == 0) {
    nrf_delay_us(2);
    detect_counter++;
    if (detect_counter >= 6) {
      result = 0; /* clk did not work */
      break;      /* escape while loop */
    }
  }
  return result;
}

#define DEBUG_EM4095 1

// 根據您的 Log 調整的數值
#define EM_SHORT_MIN 3000
#define EM_SHORT_MAX 4800
#define EM_LONG_MIN 6500
#define EM_LONG_MAX 9000

void clean_glitches(uint16_t count) {
  for (int i = 1; i < count - 1; i++) {
    if (tick_buffer[i] > 0 && tick_buffer[i] < 1500) {
      // 合併極短雜訊
      tick_buffer[i + 1] += tick_buffer[i];
      tick_buffer[i] = 0;
    }
  }
}

void decode_bitstream(uint16_t total_ticks, uint8_t* out_bits, int* out_len) {
  int bit_idx = 0;
  uint8_t current_val = 1;  // 假設 Header 為 1
  int state = 0;

  memset(out_bits, 0, 300);
  *out_len = 0;

  for (int i = 0; i < total_ticks; i++) {
    uint32_t T = tick_buffer[i];
    if (T == 0) continue;

    if (T > EM_LONG_MIN && T < EM_LONG_MAX) {
      // Long Pulse: 翻轉數值 (例如 0->1 或 1->0)
      if (state == 1) state = 0;  // 重置狀態
      current_val = !current_val;
      if (bit_idx < 300) out_bits[bit_idx++] = current_val;
      state = 0;
    } else if (T > EM_SHORT_MIN && T < EM_SHORT_MAX) {
      // Short Pulse: 保持數值
      if (state == 0) {
        state = 1;  // 等待第二個 Short
      } else {
        // 第二個 Short 到達，確認一個 Bit
        if (bit_idx < 300) out_bits[bit_idx++] = current_val;
        state = 0;
      }
    } else {
      // 異常長度，重置狀態
      state = 0;
    }
  }
  *out_len = bit_idx;
}

/* 3. 校驗與解碼 */
int check_and_decode(uint8_t* bits, uint64_t* result) {
  uint8_t data[64];

  // Header Check
  for (int k = 0; k < 9; k++)
    if (bits[k] != 1) return 0;
  // Stop Bit
  if (bits[63] != 0) return 0;

  // Copy
  for (int k = 0; k < 64; k++) data[k] = bits[k];

  // Parity Check (Full Matrix)
  uint8_t col_p[4] = {0};
  for (int r = 0; r < 10; r++) {
    int base = 9 + r * 5;
    int row_sum = 0;
    for (int c = 0; c < 4; c++) {
      row_sum += data[base + c];
      col_p[c] += data[base + c];
    }
    if ((row_sum + data[base + 4]) % 2 != 0) return 0;
  }
  for (int c = 0; c < 4; c++) {
    if ((col_p[c] + data[59 + c]) % 2 != 0) return 0;
  }

  // Decode
  *result = 0;
  for (int j = 0; j < 10; j++) {
    uint64_t digit = (data[9 + 5 * j] << 3) | (data[9 + 5 * j + 1] << 2) |
                     (data[9 + 5 * j + 2] << 1) | (data[9 + 5 * j + 3] << 0);
    *result |= (digit << (4 * (9 - j)));
  }
  return 1;
}

int EM4100_Full_Check(uint8_t* bits) {
  // 檢查 Header (9個1)
  for (int i = 0; i < 9; i++)
    if (bits[i] != 1) return 0;
  // 檢查 Stop Bit (1個0)
  if (bits[63] != 0) return 0;

  uint8_t col_parity_calc[4] = {0};

  // 檢查 Row Parity
  for (int row = 0; row < 10; row++) {
    int base = 9 + row * 5;
    int row_sum = 0;
    for (int col = 0; col < 4; col++) {
      uint8_t val = bits[base + col];
      row_sum += val;
      col_parity_calc[col] += val;
    }
    // 偶同位檢查
    if ((row_sum + bits[base + 4]) % 2 != 0) return 0;
  }

  // 檢查 Column Parity
  for (int col = 0; col < 4; col++) {
    if ((col_parity_calc[col] + bits[59 + col]) % 2 != 0) return 0;
  }
  return 1;  // 校驗通過
}

/* 4. 卡號轉換函式 (您缺失的部分) */
uint64_t decode_card_code(uint8_t* bits) {
  uint64_t code = 0;
  for (int j = 0; j < 10; j++) {
    unsigned int digit = (bits[9 + 5 * j] << 3) | (bits[9 + 5 * j + 1] << 2) |
                         (bits[9 + 5 * j + 2] << 1) |
                         (bits[9 + 5 * j + 3] << 0);
    code |= ((uint64_t)digit << (4 * (9 - j)));
  }
  /*total 40 bits, 保留最後 32 bits (8個 Hex)，過濾掉前面的 Customer ID*/
  return (code & 0xFFFFFFFF);
}

/* 4. 主接收函式 */
unsigned short em4095_em_receiver_ppi(void) {
  uint8_t raw_bits[350];
  int raw_bit_count = 0;

  // PPI 採樣
  em4095_set_trigger_mode(ASK);
  memset(tick_buffer, 0, TICK_BUFFER_SIZE * sizeof(uint32_t));

  em4095_timer_enable();
  em4095_timer_expired = 0;
  demod_counter = 0;

  em4095_gpio_sampling_enable();

  nrf_delay_ms(120);
  em4095_gpio_sampling_disable();
  em4095_timer_disable();

  em4095_set_trigger_mode(FSK);

  if (demod_counter < 64) return 0;

  clean_glitches(demod_counter);

  // 解碼
  decode_bitstream(demod_counter, raw_bits, &raw_bit_count);

  if (raw_bit_count < 64) return 0;

  // 搜尋卡號
  for (int i = 0; i <= raw_bit_count - 64; i++) {
    // 檢查 Header (九個 1)
    int header_match = 1;
    for (int k = 0; k < 9; k++)
      if (raw_bits[i + k] != 1) {
        header_match = 0;
        break;
      }
    if (!header_match) continue;

    // 提取 64 bits
    for (int k = 0; k < 64; k++) data_valid[k] = raw_bits[i + k];

    // 執行校驗
    if (EM4100_Full_Check(data_valid) == 1) {
      em_card_code = decode_card_code(data_valid);
      NRF_LOG_INFO("[em4095] Card Found: (%010llu)", em_card_code);
      tone_em4095_detected();
      return 1;
    }
  }

  return 0;
}

void em4095_em_receiver(void) {
  unsigned short preBit, currBit, wait16Clks;

  // --- [新增 1] 變數宣告與優先級設定 ---
  int old_prio;
  k_tid_t current_tid = k_current_get();  // 取得當前線程 ID

  // 1. 保存當前的優先級
  old_prio = k_thread_priority_get(current_tid);

  // 2. 將優先級設定為 -1 (Cooperative, 不可被其他線程搶佔)
  // 注意：這不會關閉硬體中斷，但會阻止其他 Task 插隊
  k_thread_priority_set(current_tid, -1);
  // ----------------------------------

  sync_flag = 0;   // sync_flag is set when falling edge on RA14 is detected
  one_seq = 0;     // counts the number of 'logic one' in series
  data_in = 0;     // gets data bit
  data_index = 0;  // marks position in data arrey
  cnt = 0;         // interrupt counter
  cnt1 = 0;        // auxiliary counter
  cnt2 = 0;        // auxiliary counter
  loopflag = 1;
  detect_bus_counter = 0;

  while (loopflag) {
    loopflag = 0;
    bad_synch = 0;  // set bad synchronization variable to zero
    cnt = 0;        // reseting interrupt counter
    sync_flag = 0;  // reseting sync flag

    /* check CLK bus */
    detect_bus_counter = 0;
    while (gpio_pin_get_dt(&clk_gpio) == 0) {
      nrf_delay_us(2);
      detect_bus_counter++;
      if (detect_bus_counter >= 6) {
        detect_bus_counter = 0;
        em4095_reboot = 1;

        // --- [新增 2] 錯誤返回前還原優先級 ---
        k_thread_priority_set(current_tid, old_prio);
        return;
      }
    }
    detect_bus_counter = 0;
    while (gpio_pin_get_dt(&demod_out_gpio) == 1) {
      nrf_delay_us(8);
      detect_bus_counter++;
      if (detect_bus_counter >= (32 * 3)) {
        // --- [新增 3] 錯誤返回前還原優先級 ---
        k_thread_priority_set(current_tid, old_prio);
        return;
      }
    }
    while (cnt != 16) {
      while (gpio_pin_get_dt(&clk_gpio) == 1) {
        __asm("nop");
      }
      while (gpio_pin_get_dt(&clk_gpio) == 0) {
        __asm("nop");
      }
      cnt++;
    }
    _data[0] = gpio_pin_get_dt(&demod_out_gpio) & 1;
    preBit = _data[0];
    wait16Clks = 0;
    for (data_index = 1; data_index <= 255; data_index++) {
      // getting 128 bits of data from DEMOD
      if (wait16Clks) {
        cnt = 16;  // reseting clock counter
      } else {
        cnt = 0;  // reseting clock counter
      }
      wait16Clks = 0;
      while (cnt != 32) {
        while (gpio_pin_get_dt(&clk_gpio) == 1) {
          __asm("nop");
        }
        while (gpio_pin_get_dt(&clk_gpio) == 0) {
          __asm("nop");
        }
        cnt++;
      }
      // reseting clock counter
      _data[data_index] = gpio_pin_get_dt(&demod_out_gpio) & 1;  // geting bit
      currBit = _data[data_index];
      if (currBit == preBit) {
        cnt = 0;
        if (currBit == 1) /* current is high, wait DEMOD to low */
        {
          while (gpio_pin_get_dt(&demod_out_gpio) == 1) {
            //__asm("nop");
            nrf_delay_us(8);
            cnt++;
            if (cnt > 32) {
              break; /* timeout and escape loop */
            }
          }
        } else /* current is low and wait DEMOD to high */
        {
          while (gpio_pin_get_dt(&demod_out_gpio) == 0) {
            //__asm("nop");
            nrf_delay_us(8);
            cnt++;
            if (cnt > 32) {
              break; /* timeout and escape loop */
            }
          }
        }
        if (cnt > 32) {
          bad_synch = 1;
          goto __BADSYNC;
        }
        wait16Clks = 1;
      }
      preBit = currBit;
    }
    for (data_index = 0; data_index <= 255; data_index++) {
      if (data_index & 1)
        if (!(_data[data_index] ^ _data[data_index - 1])) {
          // bad_synch = 1;
          break;  // bad synchronisation
        }
    }
  __BADSYNC:
    if (bad_synch) {
      // loopflag--;
      continue;  // try again
    }
    cnt1 = 0;
    one_seq = 0;
    for (cnt1 = 0; cnt1 <= 127;
         cnt1++) {  // we are counting 'logic one' in the data array
      if (_data[cnt1 << 1] == 1) {
        one_seq++;
      } else {
        one_seq = 0;
      }

      if (one_seq == 9) {  // if we get 9 'logic one' we break from the loop
        break;
      }
    }  //   (the position of the last  'logic one' is in the cnt1)

    if ((one_seq == 9) &&
        (cnt1 < 73)) {    //    if we got 9 'logic one' before cnt1 position 73
                          //   we write that data into data_valid array
      data_valid[0] = 1;  //   (it has to be before cnt1 position 73 in order
      data_valid[1] = 1;  //    to have all 64 bits available in data array)
      data_valid[2] = 1;
      data_valid[3] = 1;
      data_valid[4] = 1;
      data_valid[5] = 1;
      data_valid[6] = 1;
      data_valid[7] = 1;
      data_valid[8] = 1;
      for (cnt2 = 9; cnt2 <= 63; cnt2++) {  // copying the rest of data from the
                                            // data array into data_valid array
        cnt1++;
        data_valid[cnt2] = _data[cnt1 << 1];
      }
      if (CRC_Check(data_valid) ==
          1) {  // if data in data_valid array pass the CRC check

        em4095_detected = loopflag + 1;
        loopflag = 0;

        em_card_code = 0;
        for (i = 0; i < 10; i++) {
          em_digit = (data_valid[9 + 5 * i] << 3) |
                     (data_valid[9 + 5 * i + 1] << 2) |
                     (data_valid[9 + 5 * i + 2] << 1) |
                     (data_valid[9 + 5 * i + 3] << 0);
          em_card_code |= (em_digit << (4 * (9 - i)));
        }
        NRF_LOG_INFO("[em4095] EM4XXX Card NO: (%010llu)", em_card_code);
        // tone_em4095_detected();
      }
    }
  }

  // --- [新增 4] 正常結束前還原優先級 ---
  k_thread_priority_set(current_tid, old_prio);
}

// main program
void em4095_receiver(void) {
  if (em4095_reboot) {
    em4095_reboot = 0;
    em4095_reset();
#ifdef EM4095_SHD_HW_MOD
    /* needs 25.60ms delay at least */
    nrf_delay_ms(30);
#endif /* #ifdef EM4095_SHD_HW_MOD */
    NRF_LOG_INFO("[em4095] RESET!");
  }
#ifdef EM4095_FSK_DETECTION
  unsigned short result = em4095_hid_receiver();
  if (!result) {
    // NRF_LOG_INFO("[em4095] Trying use em!");
    //  int result = em4095_clk_check();
    //  NRF_LOG_INFO("[em4095] Check clk = %d!", result);
    em4095_em_receiver_ppi();
  } else {
    // tone_em4095_detected();
    em4095_detected = 1;
#ifdef EM4095_SHD_HW_MOD
    em4095_reboot = 1;
    /* need 25-35ms for PLL stable */
#endif /* EM4095_SHD_HW_MOD */
       /* reset em4095 again due to sometimes incorrect behavior */
  }
#else /* #ifdef EM4095_FSK_DETECTION */
  em4095_em_receiver();
#endif
}