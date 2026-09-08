#include "contiki.h"
#include "sys/node-id.h"
#include "sys/log.h"

#include "flock.h"
#include "flock-rng.h"
#include "flock-msg.h"
#include "flock-report.h"
#include "flock-dist-table.h"
#include "flock-pos-table.h"
#include "dw1000.h"
#include "dw1000-staggered-ranging.h"

#define LOG_MODULE "FLOCK"
#define LOG_LEVEL LOG_LEVEL_WARN

PROCESS(flock_process, "Flock main process");

static rtimer_clock_t sync_time = 0;
static struct rtimer* flock_timer = &dw1000_timer;   
static rtimer_clock_t rng_slot_start_time = 0;

flock_pos_table_t flock_pos_table;


/* ---------------------------------------------------------------------------- */

void 
flock_osf_sync_callback()
{
  sync_time = RTIMER_NOW();
  process_poll(&flock_process);
}

static void
flock_rng_with(struct rtimer* t, void* ptr)
{
  process_poll(&flock_rng_process);
}

/* ---------------------------------------------------------------------------- */

PROCESS_THREAD(flock_process, ev, data)
{
  PROCESS_BEGIN();

  LOG_INFO("Starting...\n");

  flock_dist_table_init();

  flock_pos_table_init(&flock_pos_table);

  process_start(&flock_rng_process, NULL);
  process_start(&flock_report_ranges, NULL);
  process_start(&flock_message_process, NULL);

  while(true) {
    PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_POLL);

    dw1000_off();
    if(dw1000_on() != DW1000_OK){
      LOG_ERR("Failed to turn on DW1000\n");
      continue;
    }

    /* Calculate start time of own ranging slot */
    uint8_t ranging_slot = (node_id);
    rng_slot_start_time = sync_time + (US_TO_RTIMERTICKS(DW1000_ST_RNG_SLOT_DURATION_US) * ranging_slot) + FLOCK_SYNC_EVENT_GUARD_TIME;
    
    /* Schedule a ranging slot */
    rtimer_set(flock_timer, rng_slot_start_time, 0, flock_rng_with, NULL); 
  }

  PROCESS_END();
}

/* ---------------------------------------------------------------------------- */