#include "flock-rng.h"

#include "flock-dist-table.h"
#include "os/net/mac/osf/osf.h"
#include "deca_device_api.h"
#include "deca_regs.h"
#include "dw1000.h"
#include "dw1000-nbr.h"
#include "dw1000-staggered-ranging.h"
#include "sys/log.h"
#include "sys/node-id.h"
#include "os/lib/random.h"

#include <string.h>

#define LOG_MODULE "FLOCK-RNG"
#define LOG_LEVEL LOG_LEVEL_WARN

PROCESS(flock_rng_process, "Flock Ranging Process");
PROCESS(flock_rng_cb_process, "Flock Ranging Callback Process");

static flock_rng_range_cb_t _range_cb = NULL;

static void shuffle(uint16_t* arr, size_t n) {
  for (size_t i = n - 1; i > 0; i--) {
    size_t j = random_rand() % n;
    uint16_t temp = arr[i];
    arr[i] = arr[j];
    arr[j] = temp;
  }
}

static size_t 
get_ranging_partner_ids(uint16_t* ranging_partners, size_t n_ids){
  size_t total_nbr_len = dw1000_nbr_table_get_length();
  size_t sel_len = 0;
  uint16_t ids[total_nbr_len];

  if(total_nbr_len == 0){
    LOG_DBG("No known neighbors found.\n");
    return 0;
  }
  LOG_DBG("Known neighbors: %d\n", total_nbr_len);

  size_t i = 0;
  for (size_t j = 0; j < DW1000_NBR_MAX; j++){
    const dw1000_nbr_t* nbr = dw1000_nbr_table_get_elem(i);
    if(nbr != NULL && nbr->is_valid){
      ids[i++] = nbr->id;
    }
  }


  shuffle(ids, total_nbr_len);

  sel_len = (n_ids < total_nbr_len) ? n_ids : total_nbr_len;

  memcpy(ranging_partners, ids, sizeof(uint16_t) * sel_len);


  LOG_DBG("Selected %d ranging partners: ", sel_len);
  for(size_t i = 0; i < sel_len; i++){
    LOG_DBG_("%d ", ranging_partners[i]);
  }
  LOG_DBG_("\n");
  return sel_len;
}


PROCESS_THREAD(flock_rng_process, ev, data)
{
  static uint16_t ranging_partner_ids[DW1000_ST_RNG_MAX_RNG_PARTNERS] = {0};
  static size_t n_rng_partners = 0;
  PROCESS_BEGIN();

  LOG_INFO("Starting...\n");

  // initialize ranging callback process
  process_start(&flock_rng_cb_process, NULL);
  dw1000_staggered_range_set_done_cb(&flock_rng_cb_process);
  
  /* seeding the random numer generator */
  random_init(node_id + RTIMER_NOW());

  while(true) {
    PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_POLL);
    
    n_rng_partners = get_ranging_partner_ids(ranging_partner_ids, DW1000_ST_RNG_MAX_RNG_PARTNERS);
    LOG_DBG("%u/%u ranging partners selected.\n", n_rng_partners, DW1000_ST_RNG_MAX_RNG_PARTNERS);

    if(n_rng_partners == 0){
      LOG_DBG("No known ranging partners found. Send empty ranging request.\n");
      uint16_t no_partners[1] = {0};
      dw1000_staggered_range_with(no_partners, 1);
    }else{
      dw1000_staggered_range_with(ranging_partner_ids, n_rng_partners);
    }
  }
  PROCESS_END();
}

PROCESS_THREAD(flock_rng_cb_process, ev, data)
{
  PROCESS_BEGIN();

  static uint16_t from_id = 0;
  static uint16_t to_id = 0;
  static uint16_t dist_mm = 0.0f;
  static uint16_t osf_epoch = 0;
  static rtimer_clock_t calc_rtime = 0;

  LOG_INFO("Flock RNG Callback Process started.\n");

  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_POLL);
    LOG_DBG("Ranging callback triggered.\n");

    calc_rtime = ((unsigned) RTIMER_NOW());
    const rng_result_t* result = (const rng_result_t*) data;
    from_id = result->from;
    to_id = result->to;
    dist_mm = result->dist_dstwr_mm;
    osf_epoch = osf.epoch;
    if(result != NULL) {
      LOG_DBG("Ranging result: from %u to %u, distance: %ld mm, epoch %u\n", from_id, to_id, (int32_t) dist_mm, osf_epoch);
      flock_dist_table_add(from_id, to_id, (uint16_t) (dist_mm), osf_epoch);
#if FLOCK_PRINT_RANGE_RESULT
      printf("{\"type\":\"rng_result\",\"from\":%u,\"to\":%u,\"distance_mm\":%ld,\"distance_sstwr_mm\":%ld,\"distance_sstwr_corr_mm\":%ld,\"cfo_ppb\":%ld,\"slot_number\":%u,\"epoch\":%u}\n",
             from_id, to_id, (int32_t) dist_mm, (int32_t) result->dist_sstwr_mm, (int32_t) (result->dist_sstwr_corr_mm), (int32_t) (result->cfo_ppm * 1000), result->slot_number, osf_epoch);
#endif
    }
    calc_rtime = RTIMER_CLOCK_DIFF(RTIMER_NOW(), calc_rtime);

    if(_range_cb) {
      _range_cb(from_id, to_id, dist_mm, osf_epoch);
    }

    LOG_DBG("Ranging callback processing took %lu us\n", RTIMERTICKS_TO_US(calc_rtime));
  }

  PROCESS_END();
}

int flock_rng_register_range_callback(flock_rng_range_cb_t cb)
{
  if(_range_cb && (_range_cb != cb)) {
    /* A different callback already registered */
    return 1;  
  }

  _range_cb = cb;
  return 0;
}

int flock_rng_deregister_range_callback(flock_rng_range_cb_t cb)
{
  if(!_range_cb) {
    /* No callback registered, OK */
    return 0;
  }

  if(_range_cb != cb) {
    /* Registered callback does not match users */
    return 1;
  }

  _range_cb = NULL;
  return 0;
}