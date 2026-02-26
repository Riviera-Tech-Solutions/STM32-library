#ifndef HEALTH_SUPERVISOR_H
#define HEALTH_SUPERVISOR_H

#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"

#ifndef HS_MAX_WATCHED_TASKS
#define HS_MAX_WATCHED_TASKS 8U
#endif

typedef enum {
  HS_ERR_NONE = 0,
  HS_ERR_INVALID_INDEX,
  HS_ERR_NOT_CONFIGURED
} hs_error_t;

typedef struct {
  uint32_t checks;
  uint32_t faults_detected;
  uint32_t recoveries_ok;
  uint32_t recoveries_fail;
  hs_error_t last_error;
} hs_stats_t;

typedef bool (*hs_recover_cb_t)(void);

bool hs_register_watch(uint8_t slot, TickType_t max_silence_ticks, hs_recover_cb_t recover_cb);
void hs_heartbeat(uint8_t slot);
void hs_supervisor_task(void *arg);
void hs_get_stats(hs_stats_t *out_stats);
void hs_reset_stats(void);

#endif /* HEALTH_SUPERVISOR_H */
