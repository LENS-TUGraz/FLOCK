#include "flock-msg.h"

#include "flock-dist-table.h"

#include <string.h>
#include "net/mac/osf/osf.h"
#include "net/mac/osf/osf-packet.h"


/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "FLOCK MSG"
#define LOG_LEVEL LOG_LEVEL_WARN

static flock_msg_t tx_msg;
static uint8_t tx_buffer[OSF_DATA_LEN_MAX] = {0};

static flock_msg_t rx_msg;
static uint8_t rx_buffer[OSF_DATA_LEN_MAX] = {0};

PROCESS(flock_message_process, "Flock Message Process");
PROCESS(flock_process_report_msg_process, "Flock Process Report Message Process");

/*---------------------------------------------------------------------------*/
void flock_osf_input_callback(uint8_t src, uint8_t dest, uint8_t *data, uint8_t len){
  memset(&rx_msg, 0, sizeof(rx_msg));

  rx_msg.from = src;
  rx_msg.to = dest;
  rx_msg.type = *((uint16_t*) &(data[FLOCK_MSG_TYPE_OFFSET]));
  rx_msg.data = rx_buffer;
  rx_msg.data_len = len - FLOCK_MSG_TYPE_SIZE;

  memcpy((uint8_t*) rx_buffer, (uint8_t*) &data[FLOCK_MSG_DATA_OFFSET], rx_msg.data_len);

  process_poll(&flock_message_process);
}

void 
flock_msg_send( uint16_t from, 
                uint16_t to, 
                uint16_t type, 
                uint8_t* data, 
                size_t data_len)
{
  size_t tx_msg_size = FLOCK_MSG_TYPE_SIZE + data_len;

  if (tx_msg_size > OSF_DATA_LEN_MAX) {
    LOG_ERR("Not enough space in tx_buffer\n");
    LOG_ERR("tx_buffer size: %u, tx_msg_size: %u\n", sizeof(tx_buffer), tx_msg_size);
    return;
  }
  memset(tx_buffer, 0, sizeof(tx_buffer));

  tx_msg.from = from;
  tx_msg.to = to;
  tx_msg.type = type;
  tx_msg.data = tx_buffer;
  tx_msg.data_len = data_len;

  memcpy((uint8_t*) &tx_buffer[FLOCK_MSG_TYPE_OFFSET], (uint8_t*) &(tx_msg.type), FLOCK_MSG_TYPE_SIZE);
  memcpy((uint8_t*) &tx_buffer[FLOCK_MSG_DATA_OFFSET], (uint8_t*) data, tx_msg.data_len);

  LOG_DBG("TX message: ");
  for(size_t i = 0; i < tx_msg_size; i++){
    LOG_DBG_("%02x ", tx_buffer[i]);
  }
  LOG_DBG_("\n");

  LOG_INFO("Sending message from %u to %u of type 0x%04X len %u\n", (uint8_t) tx_msg.from, (uint8_t) tx_msg.to, tx_msg.type, tx_msg_size);
  osf_send(tx_buffer, tx_msg_size, (uint8_t) tx_msg.to);
}

#if FLOCK_PRINT_RANGE_REPORT
static void
print_range_report(uint16_t from_id, uint16_t to_id, uint16_t dist_mm, uint16_t epoch)
{
  printf("[FLOCK RNG] {\"from\":%u, \"to\":%u, \"dist_mm\":%u,\"epoch\":%u}\n",
         from_id, to_id, dist_mm, epoch);
}
#endif /* FLOCK_PRINT_RANGE_REPORT */

PROCESS_THREAD(flock_process_report_msg_process, ev, data)
{
  PROCESS_BEGIN();
  flock_msg_t* msg = (flock_msg_t*) data;

  LOG_DBG("FLOCK Report Message Process started\n");

  LOG_DBG("Processing report message from %u to %u\n", msg->from, msg->to);
  static uint16_t rng_to_id;
  static uint16_t rng_from_id;
  static uint16_t rng_dist_mm;
  static uint16_t rng_osf_epoch;
  static uint16_t data_length;
  static size_t n_elements;
  static size_t i;
  static uint8_t buf[250];

  rng_to_id = msg->from;
  data_length = msg->data_len;

  n_elements = (data_length) / (3*sizeof(uint16_t));
  LOG_DBG("Number of elements: %u\n", n_elements);

  memcpy(buf, msg->data, data_length);

  for(i = 0; i < n_elements; i++){
    rng_from_id = *((uint16_t*)     &buf[i*3*sizeof(uint16_t)]);
    rng_dist_mm = *((uint16_t*)     &buf[i*3*sizeof(uint16_t) + sizeof(uint16_t)]);
    rng_osf_epoch = *((uint16_t*)   &buf[i*3*sizeof(uint16_t) + 2*sizeof(uint16_t)]);

    flock_dist_table_add(rng_from_id, rng_to_id, rng_dist_mm, rng_osf_epoch);

#if FLOCK_PRINT_RANGE_REPORT
    print_range_report(rng_from_id, rng_to_id, rng_dist_mm, rng_osf_epoch);
#endif

    LOG_DBG("Ranging from %u to %u: %u mm, epoch %u\n", rng_from_id, rng_to_id, rng_dist_mm, rng_osf_epoch);
  }

  LOG_DBG("\n");
  PROCESS_END();
}

PROCESS_THREAD(flock_message_process, ev, data)
{
  PROCESS_BEGIN();

  LOG_INFO("Flock message process started\n");

  while(true){
    PROCESS_YIELD_UNTIL(ev == PROCESS_EVENT_POLL);

    LOG_DBG("RX message from %u to %u of type 0x%04X length: %u\n", (uint8_t) rx_msg.from, (uint8_t) rx_msg.to, rx_msg.type, rx_msg.data_len);
    LOG_DBG("data: ");
    for(size_t i = 0; i < rx_msg.data_len; i++){
      LOG_DBG_("%02x ", rx_buffer[i]);
    }
    LOG_DBG_("\n");

    switch(rx_msg.type){
      case FLOCK_REPORT:
        process_start(&flock_process_report_msg_process, &rx_msg);
        break;
      default:
        LOG_ERR("Unknown message type: %u\n", rx_msg.type);
        break;
    };
  }

  PROCESS_END();
}