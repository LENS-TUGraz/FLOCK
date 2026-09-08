#ifndef _FLOCK_RNG_H_
#define _FLOCK_RNG_H_

#include "contiki.h"

typedef void (*flock_rng_range_cb_t)(uint16_t from_id, uint16_t to_id, uint16_t dist_mm, uint16_t osf_epoch);

PROCESS_NAME(flock_rng_process);
PROCESS_NAME(flock_rng_cb_process);

void flock_osf_sync_callback();

int flock_rng_register_range_callback(flock_rng_range_cb_t cb);

int flock_rng_deregister_range_callback(flock_rng_range_cb_t cb);

#endif