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

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include "hubble_advertiser.h"
#include <b64/cdecode.h>
#include <hubble/ble.h>
#include <hubble/hubble_port.h>

/*
 * These should be set in the makefile
 */
#ifndef HUBBLE_KEY_B64_STR
#define HUBBLE_KEY_B64_STR ""
#endif
#ifndef HUBBLE_UTC_TIME_MS
#define HUBBLE_UTC_TIME_MS 0
#endif

/*
 * We will decode the HUBBLE_KEY_B64_STR config into this
 */
static uint8_t master_key[CONFIG_HUBBLE_KEY_SIZE];

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

static void hubble_init(void) {
  // Decode the base64 string to the master key
  // This is optional and done to make it easier to pass keys in
  base64_decodestate s;
  base64_init_decodestate(&s);
  size_t cnt = base64_decode_block(HUBBLE_KEY_B64_STR,
                                   strlen(HUBBLE_KEY_B64_STR), master_key, &s);
  if (cnt != CONFIG_HUBBLE_KEY_SIZE) {
    NRF_LOG_ERROR("Incorrect key size passed");
    return;
  }

  hubble_ble_init(HUBBLE_UTC_TIME_MS);
  hubble_ble_key_set(master_key);

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
  hubble_init();

  // Start execution.
  NRF_LOG_INFO("Hubble reference started.");

  bsp_indication_set(BSP_INDICATE_ADVERTISING);

  // Enter main loop.
  for (;;) {
    idle_state_handle();
  }
}
