#include "flock-conf.h"
#include "flock-report.h"
#include "flock-msg.h"
#include "flock-dist-table.h"
#include "dw1000-nbr.h"
#include "net/mac/osf/osf-packet.h"
#include "sys/node-id.h"

#include "sys/log.h"
#define LOG_MODULE "FLOCK REPORT"
#define LOG_LEVEL LOG_LEVEL_WARN

PROCESS(flock_report_ranges, "Flock Report Ranges");

/* ------------------------------------------------------------------- */
static uint8_t report_buffer[OSF_DATA_LEN_MAX];

/* ------------------------------------------------------------------- */
static struct pt prune_pt;
void
flock_report_send()
{
  uint16_t* rng_data = (uint16_t*) report_buffer;
  const flock_dist_table_element_t* el;
  size_t rng_data_idx = 0;

  while(flock_dist_table_prune(&prune_pt, FLOCK_LOC_EPOCHS_TO_KEEP) < PT_EXITED){};

  el = flock_dist_table_get_head();
  while(el != NULL){
    if(el->is_valid && (el->to_id == node_id)) {
      LOG_DBG("Reporting distance from %u to %u: %u mm\n", 
               el->from_id, el->to_id, el->dist_mm);

      rng_data[rng_data_idx++] = el->from_id;
      rng_data[rng_data_idx++] = el->dist_mm;
      rng_data[rng_data_idx++] = el->osf_epoch;
    }

    el = flock_dist_table_get_next(el);
  }
 
  if(rng_data_idx == 0){
    LOG_DBG("No valid neighbors to report\n");
    return;
  }

  LOG_DBG("Sending report with %u elements\n", rng_data_idx / 2);
  flock_msg_send(node_id, 0xFF, FLOCK_REPORT, report_buffer, rng_data_idx * 2);
}

/* ------------------------------------------------------------------- */
PROCESS_THREAD(flock_report_ranges, ev, data)
{
  static struct etimer et;
  PROCESS_BEGIN();

  etimer_set(&et, FLOCK_REPORT_INTERVAL);

  while(1){
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    etimer_reset(&et);
    flock_report_send();
  }

  PROCESS_END();
}