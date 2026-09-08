/**
 * \file
 *         FLOCK main file.
 */

#include "contiki.h"
#include "contiki-net.h"
#include "net/mac/osf/osf.h"
#include "sys/node-id.h"

#include "dw1000.h"
#include "flock.h"
#include "flock-loc.h"

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

#include "sys/int-master.h"

/*---------------------------------------------------------------------------*/
PROCESS(main_process, "Flock Main Process");
AUTOSTART_PROCESSES(&main_process);



/*---------------------------------------------------------------------------*/

void print_osf_status(){
  printf("{");
    printf("\"epoch\": %u, ", osf.epoch);
    printf("\"join_epoch\": %u, ", osf.join_epoch);
    printf("\"n_tx\": %u, ", osf.n_tx);
    printf("\"n_rx_ok\": %u, ", osf.n_rx_ok);
    printf("\"n_rx_crc\": %u, ", osf.n_rx_crc);
    printf("\"slot\": %u, ", osf.slot);
    printf("\"last_rx_ok\": %u, ", osf.last_rx_ok);
    printf("\"last_sync_slot\": %u, ", osf.last_sync_slot);
    printf("\"last_slot_type\": \"%c\", ", osf.last_slot_type);
    printf("\"n_syncs\": %u, ", osf.n_syncs);
    printf("\"n_rnd_since_rx\": %u, ", osf.n_rnd_since_rx);
    printf("\"failed_epochs\": %u, ", osf.failed_epochs);
    printf("\"t_epoch_ref\": %lu, ", osf.t_epoch_ref);
    printf("\"t_epoch_drift\": %d, ", osf.t_epoch_drift);
    printf("\"t_slot_drift\": %d", osf.t_slot_drift);
  printf("}");
}

void print_uwb_state(){
  uint32_t state = dw1000_get_state();
  uint8_t psmc_state = (state & 0x000F0000) >> 16;

  printf("{");
    printf("\"tx_state\": \"%s\", ", (state & 0x0000000F) ? "TX" : "IDLE");
    printf("\"rx_state\": \"%s\", ", (state & 0x00000700) ? "RX" : "IDLE");
    printf("\"psmc_state\": ");
      switch(psmc_state){
        case 0x0: printf("\"INIT\""); break;
        case 0x1: printf("\"IDLE\""); break;
        case 0x2: printf("\"TX_WAIT\""); break;
        case 0x3: printf("\"RX_WAIT\""); break;
        case 0x4: printf("\"TX\""); break;
        case 0x5: printf("\"RX\""); break;
        default: printf("\"UNKNOWN\""); break;
      }
  printf("}");
}

void print_uwb_status(){
  uint32_t status = dw1000_get_status();

  printf("{");
    printf("\"status\": \"0x%08lX\", ", status);
    printf("\"state\": ");
      print_uwb_state();
  printf("}");
}

void print_interrupt_status(){
  printf("{");
    printf("\"primask\": %lu, ", __get_PRIMASK());
    printf("\"basepri\": %lu, ", __get_BASEPRI());
    printf("\"interrupts_enabled\": %s", int_master_is_enabled() ? "true" : "false");
  printf("}");
}



/*---------------------------------------------------------------------------*/
PROCESS_THREAD(main_process, ev, data)
{
  PROCESS_BEGIN();
  static struct etimer et;
  LOG_INFO("Starting...\n");

  if(dw1000_init() != DW1000_OK) {
    /* If the radio failed to initialise, nothing else can work, so reset */
    watchdog_reboot();
    PROCESS_EXIT();
  }

  process_start(&flock_process, NULL);

  /* Register a callback so we can receive from OSF */
  osf_register_input_callback(flock_osf_input_callback);

  /* Start OSF (will have been initialised in netstack.c) */
  NETSTACK_MAC.on();

#if FLOCK_LOC_UPDATE_RATE
  /* Start periodic localization */
  process_start(&flock_loc_process, NULL);
#endif
  etimer_set(&et, CLOCK_SECOND * 1);
  while(1){
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    etimer_reset(&et);

    if(!int_master_is_enabled()) {
      LOG_ERR("CRITICAL: Interrupts were disabled! Re-enabling...\n");
      int_master_enable();
    }

#if LOG_LEVEL == LOG_LEVEL_DEBUG
    printf("{");
      printf("\"node_id\": %u, ", node_id);
      printf("\"type\": \"keepalive\", ");
      printf("\"osf\": ");
        print_osf_status();
      printf(", \"uwb\": ");
        print_uwb_status();
      printf(", \"irq\": ");
        print_interrupt_status();
    printf("}\n");
#endif
  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
