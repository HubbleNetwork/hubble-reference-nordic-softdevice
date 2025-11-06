#include "hubble_advertiser.h"
#include "nrf_sdh_ble.h"
#include <errno.h>
#include <hubble/ble.h>
#include <hubble/hubble_port.h>

// Set up Hubble protocol advertisement buffer - this will get adjusted later
#define AD_TYPE_COMPLETE_16BIT_UUIDS 0x03
#define AD_TYPE_SERVICE_DATA 0x16
static uint8_t _hubble_adv_buffer[BLE_GAP_ADV_SET_DATA_SIZE_MAX] = {
    0x03, AD_TYPE_COMPLETE_16BIT_UUIDS, 0xA6, 0xFC, 0x01, AD_TYPE_SERVICE_DATA,
};

static uint8_t m_adv_handle = BLE_GAP_ADV_SET_HANDLE_NOT_SET;
static ble_gap_adv_params_t m_gap_params;
static volatile bool m_started = false;

int hubble_advertiser_init(const hubble_advertiser_config_t *config) {
  if (config == NULL) {
    return -EINVAL;
  }

  m_gap_params.properties.type =
      BLE_GAP_ADV_TYPE_NONCONNECTABLE_NONSCANNABLE_UNDIRECTED;
  m_gap_params.p_peer_addr = NULL; // undirected
  m_gap_params.filter_policy = BLE_GAP_ADV_FP_ANY;
  m_gap_params.interval = config->interval_min;
  m_gap_params.duration = 0;                    // never time out
  m_gap_params.primary_phy = BLE_GAP_PHY_1MBPS; // legacy uses 1M
  m_gap_params.max_adv_evts = 0;

  return hubble_advertiser_update_data(NULL, 0);
}

int hubble_advertiser_deinit(void) {
  // Unused for now
  return 0;
}

int hubble_advertiser_start(void) {
  m_started = true;
  ret_code_t err = sd_ble_gap_adv_start(m_adv_handle, APP_BLE_CONN_CFG_TAG);
  return -(int)err;
}

int hubble_advertiser_stop(void) {
  if (m_adv_handle == BLE_GAP_ADV_SET_HANDLE_NOT_SET) {
    return -EAGAIN;
  }
  m_started = false;
  ret_code_t err = sd_ble_gap_adv_stop(m_adv_handle);
  return -(int)err;
}

int hubble_advertiser_update_data(const uint8_t *data, size_t len) {
  size_t out_len = sizeof(_hubble_adv_buffer) - 6;
  if (hubble_ble_advertise_get(data, len, &_hubble_adv_buffer[6], &out_len) !=
      0) {
    HUBBLE_LOG_WARNING("Failed to generate Hubble advertisement");
    return -EINVAL;
  }
  _hubble_adv_buffer[4] = out_len + 1;

  ble_gap_adv_data_t gap_data = {
      .adv_data = {.p_data = _hubble_adv_buffer, .len = out_len + 6},
      .scan_rsp_data = {.p_data = NULL, .len = 0}};

  bool start_after = m_started;
  if (m_started) {
    hubble_advertiser_stop();
  }

  // (Re)configure the set and start advertising.
  ret_code_t err =
      sd_ble_gap_adv_set_configure(&m_adv_handle, &gap_data, &m_gap_params);
  if (err != NRF_SUCCESS) {
    HUBBLE_LOG_WARNING("Failed to set advertisement configuration (err=%lu)",
                       err);
    return -(int)err;
  }

  if (start_after) {
    hubble_advertiser_start();
  }

  return 0;
}
