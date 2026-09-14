#include "app_timer.h"
#include "app_util_platform.h"
#include "drv_rtc.h"
#include "nrf_log.h"
#include <hubble/port/sys.h>
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

/*
 * Internal SDK lock (required since SDK v3.0.0).
 *
 * hubble_init() calls hubble_lock_init(), and hubble_ble_advertise_get()
 * takes the lock around sequence-number allocation and nonce validation. Two
 * contexts reach that path here -- the main thread via
 * hubble_advertiser_init() and the app_timer handler via
 * adv_update_timer_handler() -- so the protection is real.
 *
 * A SoftDevice critical region is the right primitive for this application:
 * it is cheap, it cannot deadlock when taken from an interrupt handler, and
 * the only code it guards (the SDK's default RAM sequence counter) never
 * blocks. If you replace hubble_sequence_counter_get() with an
 * implementation that can block -- persisting the counter to flash, say --
 * swap this for a blocking-capable recursive mutex instead, per the contract
 * in hubble/port/sys.h.
 *
 * CRITICAL_REGION_ENTER/EXIT cannot be used directly: under SOFTDEVICE_PRESENT
 * they are brace-scoped macros that keep the nesting flag in a local, so they
 * cannot be split across two functions. We call the underlying functions and
 * carry the nesting state ourselves.
 */
static uint8_t m_lock_depth;
static uint8_t m_lock_nested;

int hubble_lock_init(void) {
  // Nothing to set up: the lock state is statically zero-initialized.
  return 0;
}

void hubble_lock(void) {
  uint8_t nested = 0;
  app_util_critical_region_enter(&nested);

  // Safe to touch below: we are inside the critical region.
  if (m_lock_depth++ == 0) {
    // Remember whether the caller was *already* inside a critical region, so
    // the outermost unlock does not leave a region we did not enter.
    m_lock_nested = nested;
  }
}

void hubble_unlock(void) {
  if (m_lock_depth == 0) {
    // Unbalanced unlock; nothing to release.
    return;
  }

  if (--m_lock_depth > 0) {
    // Inner nesting level: stay in the region.
    app_util_critical_region_exit(1);
    return;
  }

  app_util_critical_region_exit(m_lock_nested);
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
  case HUBBLE_LOG_ERROR:
    NRF_LOG_ERROR("%s", owned);
    break;
  default:
    break;
  }

  return 0;
}
