#include "app_timer.h"
#include "ble_advdata.h"
#include "bsp.h"
#include "nordic_common.h"
#include "nrf_pwr_mgmt.h"
#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "nrf_soc.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include "hubble_advertiser.h"
#include <b64/cdecode.h>
#include <hubble/hubble.h>
#include <hubble/port/sys.h>

/*
 * This should be set in the makefile
 */
#ifndef HUBBLE_KEY_B64_STR
#define HUBBLE_KEY_B64_STR ""
#endif

/*
 * We will decode the HUBBLE_KEY_B64_STR config into this
 */
static uint8_t master_key[CONFIG_HUBBLE_KEY_SIZE];

/*
 * Bounds for decoding HUBBLE_KEY_B64_STR.
 *
 * Base64 encodes 3 bytes per 4 characters, so a correctly encoded
 * CONFIG_HUBBLE_KEY_SIZE-byte key is at most KEY_B64_MAX_CHARS characters
 * including '=' padding (44 for a 32-byte key).
 *
 * libb64 ignores padding and emits 6 bits per input character, so that many
 * characters can decode to slightly more than CONFIG_HUBBLE_KEY_SIZE bytes
 * (33 for a 32-byte key). KEY_DECODE_BUF_SIZE covers that worst case so the
 * decode cannot overrun its destination before the length is validated.
 *
 * The trailing +1 is for libb64 itself: base64_decode_block() ends a block by
 * saving the byte it would write next (state_in->plainchar = *plainchar),
 * which reads one past the last byte decoded. Without the slack, a
 * maximum-length key reads off the end of the buffer.
 */
#define KEY_B64_MAX_CHARS (((CONFIG_HUBBLE_KEY_SIZE + 2) / 3) * 4)
#define KEY_DECODE_BUF_SIZE (((KEY_B64_MAX_CHARS * 6) / 8) + 1)

/*
 * The interval at which we will update what is in the
 * advertisement packet
 */
#define ADV_UPDATE_INTERVAL_MS (1000 * 60 * 5)
APP_TIMER_DEF(m_adv_update_tmr);

static void adv_update_timer_handler(void *context) {
  // Arbitrarily make the payload be a string containing
  // the time in seconds (updated each update)
  char buf[16];
  snprintf(buf, sizeof(buf), "%lu", (uint32_t)hubble_uptime_get() / 1000);
  hubble_advertiser_update_data((const uint8_t *)buf, strlen(buf));
}

void assert_nrf_callback(uint16_t line_num, const uint8_t *p_file_name) {
  app_error_handler(0xDEADBEEF, line_num, p_file_name);
}

static void ble_stack_init(void) {
  ret_code_t err_code;

  err_code = nrf_sdh_enable_request();
  APP_ERROR_CHECK(err_code);

  // Configure the BLE stack using the default settings.
  // Fetch the start address of the application RAM.
  uint32_t ram_start = 0;
  err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
  APP_ERROR_CHECK(err_code);

  // Enable BLE stack.
  err_code = nrf_sdh_ble_enable(&ram_start);
  APP_ERROR_CHECK(err_code);
}

static void log_init(void) {
  ret_code_t err_code = NRF_LOG_INIT(NULL);
  APP_ERROR_CHECK(err_code);

  NRF_LOG_DEFAULT_BACKENDS_INIT();
}

static void leds_init(void) {
  ret_code_t err_code = bsp_init(BSP_INIT_LEDS, NULL);
  APP_ERROR_CHECK(err_code);
}

static void timers_init(void) {
  ret_code_t err_code = app_timer_init();
  APP_ERROR_CHECK(err_code);
}

static void power_management_init(void) {
  ret_code_t err_code;
  err_code = nrf_pwr_mgmt_init();
  APP_ERROR_CHECK(err_code);
}

static void idle_state_handle(void) {
  if (NRF_LOG_PROCESS() == false) {
    nrf_pwr_mgmt_run();
  }
}

static void hubble_stack_init(void) {
  // Decode the base64 string to the master key
  // This is optional and done to make it easier to pass keys in
  //
  // Decode into a scratch buffer rather than straight into master_key: libb64
  // writes as it goes and has no output bound, so an over-long KEY would run
  // past the end of the destination before we ever got to check the length.
  // Base64 yields at most 3 bytes per 4 input chars, so cap the input length
  // that can produce a CONFIG_HUBBLE_KEY_SIZE key and reject anything longer.
  const size_t key_b64_len = strlen(HUBBLE_KEY_B64_STR);
  if (key_b64_len > KEY_B64_MAX_CHARS) {
    NRF_LOG_ERROR("Key too long: %u encoded chars (max %u)",
                  (unsigned)key_b64_len, (unsigned)KEY_B64_MAX_CHARS);
    return;
  }

  uint8_t key_buf[KEY_DECODE_BUF_SIZE];
  base64_decodestate s;
  base64_init_decodestate(&s);
  size_t cnt =
      base64_decode_block(HUBBLE_KEY_B64_STR, key_b64_len, key_buf, &s);
  if (cnt != CONFIG_HUBBLE_KEY_SIZE) {
    NRF_LOG_ERROR("Incorrect key size: decoded %u bytes, expected %u",
                  (unsigned)cnt, (unsigned)CONFIG_HUBBLE_KEY_SIZE);
    return;
  }
  memcpy(master_key, key_buf, CONFIG_HUBBLE_KEY_SIZE);

  // DEVICE_UPTIME counter source: the EID counter is derived from device
  // uptime, so no wall-clock time is needed. The argument is the initial
  // counter value; start a fresh device at 0. (To keep the EID counter
  // continuous across reboots instead, persist hubble_counter_get() to flash
  // and pass the saved value here.)
  hubble_init(0, master_key);

  hubble_advertiser_config_t config = {
      .interval_min = APP_ADV_INTERVAL,
      .interval_max = APP_ADV_INTERVAL,
  };
  hubble_advertiser_init(&config);
  hubble_advertiser_start();

  // Periodically update the advertisement. This must be updated
  // at least daily to maintain the security model.
  app_timer_create(&m_adv_update_tmr, APP_TIMER_MODE_REPEATED,
                   adv_update_timer_handler);
  app_timer_start(m_adv_update_tmr, APP_TIMER_TICKS(ADV_UPDATE_INTERVAL_MS),
                  NULL);
}

int main(void) {
  log_init();
  timers_init();
  leds_init();
  power_management_init();
  ble_stack_init();
  hubble_stack_init();

  // Start execution.
  NRF_LOG_INFO("Hubble reference started.");

  bsp_indication_set(BSP_INDICATE_ADVERTISING);

  // Enter main loop.
  for (;;) {
    idle_state_handle();
  }
}
