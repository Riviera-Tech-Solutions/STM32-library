#include "health_supervisor.h"
#include <stddef.h>

#ifndef HS_SUPERVISOR_PERIOD_MS
#define HS_SUPERVISOR_PERIOD_MS 1000U
#endif

typedef struct {
  bool configured;
  TickType_t max_silence_ticks;
  TickType_t last_heartbeat_tick;
  hs_recover_cb_t recover_cb;
} hs_watch_entry_t;

static hs_watch_entry_t s_watch_entries[HS_MAX_WATCHED_TASKS];
static hs_stats_t s_hs_stats;

static bool hs_valid_slot(uint8_t slot) {
  return slot < HS_MAX_WATCHED_TASKS;
}

bool hs_register_watch(uint8_t slot, TickType_t max_silence_ticks, hs_recover_cb_t recover_cb) {
  if (!hs_valid_slot(slot) || max_silence_ticks == 0U) {
    s_hs_stats.last_error = HS_ERR_INVALID_INDEX;
    return false;
  }
  s_watch_entries[slot].configured = true;
  s_watch_entries[slot].max_silence_ticks = max_silence_ticks;
  s_watch_entries[slot].last_heartbeat_tick = xTaskGetTickCount();
  s_watch_entries[slot].recover_cb = recover_cb;
  s_hs_stats.last_error = HS_ERR_NONE;
  return true;
}

void hs_heartbeat(uint8_t slot) {
  if (!hs_valid_slot(slot)) return;
  if (!s_watch_entries[slot].configured) return;
  s_watch_entries[slot].last_heartbeat_tick = xTaskGetTickCount();
}

void hs_supervisor_task(void *arg) {
  (void)arg;
  const TickType_t check_period = pdMS_TO_TICKS(HS_SUPERVISOR_PERIOD_MS);
  for (;;) {
    TickType_t now = xTaskGetTickCount();
    for (uint8_t i = 0U; i < HS_MAX_WATCHED_TASKS; i++) {
      hs_watch_entry_t *entry = &s_watch_entries[i];
      if (!entry->configured) continue;
      s_hs_stats.checks++;
      TickType_t elapsed = now - entry->last_heartbeat_tick;
      if (elapsed <= entry->max_silence_ticks) continue;
      s_hs_stats.faults_detected++;
      if (entry->recover_cb != NULL) {
        if (entry->recover_cb()) {
          s_hs_stats.recoveries_ok++;
          entry->last_heartbeat_tick = now;
        } else {
          s_hs_stats.recoveries_fail++;
        }
      } else {
        s_hs_stats.last_error = HS_ERR_NOT_CONFIGURED;
      }
    }
    vTaskDelay(check_period);
  }
}

void hs_get_stats(hs_stats_t *out_stats) {
  if (out_stats == NULL) return;
  *out_stats = s_hs_stats;
}

void hs_reset_stats(void) {
  s_hs_stats.checks = 0U;
  s_hs_stats.faults_detected = 0U;
  s_hs_stats.recoveries_ok = 0U;
  s_hs_stats.recoveries_fail = 0U;
  s_hs_stats.last_error = HS_ERR_NONE;
}
