#include "app_timer.h"
#include "app_util_platform.h"
#include "drv_rtc.h"
#include "nrf_log.h"
#include <hubble/hubble_port.h>
#include <stdarg.h>

static inline uint32_t app_timer_hz(void) {
  // In sdk_config.h: APP_TIMER_CONFIG_RTC_FREQUENCY = N  (0..3/7 depending on
  // SDK) RTC freq = 32768 >> N
  return (32768u >> APP_TIMER_CONFIG_RTC_FREQUENCY);
}

uint64_t hubble_uptime_get(void) {
  static volatile uint32_t prev_ticks = 0;
  static uint64_t extended_ticks = 0;
  uint64_t ticks = app_timer_cnt_get();

  // Account for the timer rolling over
  CRITICAL_REGION_ENTER();
  if (ticks < prev_ticks) {
    extended_ticks += DRV_RTC_MAX_CNT + 1;
  }
  prev_ticks = ticks;
  ticks += extended_ticks;
  CRITICAL_REGION_EXIT();

  return (ticks * 1000ull) / app_timer_hz();
}

#define HUBBLE_LOG_BUF_SIZE 128
int hubble_log(enum hubble_log_level level, const char *fmt, ...) {
  char buf[HUBBLE_LOG_BUF_SIZE];
  va_list ap;
  va_start(ap, fmt);
  int needed = vsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);

  if (needed < 0) {
    // Formatting error
    return needed;
  }

  // If deferred logging is enabled, push the text so it stays valid
  // until the backend transmits it.
  const char *owned = NRF_LOG_PUSH(buf);

  switch (level) {
  case HUBBLE_LOG_DEBUG:
    NRF_LOG_DEBUG("%s", owned);
    break;
  case HUBBLE_LOG_INFO:
    NRF_LOG_INFO("%s", owned);
    break;
  case HUBBLE_LOG_WARNING:
    NRF_LOG_WARNING("%s", owned);
    break;
  case HUBBLE_LOG_ERR:
    NRF_LOG_ERROR("%s", owned);
    break;
  default:
    break;
  }

  return 0;
}
