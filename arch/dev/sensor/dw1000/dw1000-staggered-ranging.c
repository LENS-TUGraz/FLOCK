#include "dw1000-staggered-ranging.h"

#include "contiki.h"
#include "sys/log.h"
#include "sys/node-id.h"
#include "sys/rtimer.h"
#include "dw1000.h"
#include "dw1000-nbr.h"
#include "dw1000-msg.h"
#include "dw1000-conf.h"
#include "deca_range_tables.h"

#include <string.h>

#define LOG_MODULE "DW1000 STAGGERED RANGING"
#define LOG_LEVEL LOG_LEVEL_ERR

/*---------------------------------------------------------------------------*/
static dw1000_ret_t dw1000_staggered_range_process_request(const dw1000_msg_t* msg);
static dw1000_ret_t dw1000_staggered_range_process_response(const dw1000_msg_t* msg);
static dw1000_ret_t dw1000_staggered_range_process_final(const dw1000_msg_t* msg);

static dw1000_ret_t dw1000_prepare_range_request_msg(uint16_t* ids, size_t n_ids, dw1000_msg_t* msg);
static dw1000_ret_t dw1000_prepare_range_response_msg(uint16_t to, uint64_t rx_request_ts, uint64_t tx_response_ts, uint8_t slot_number, dw1000_msg_t* msg);
static dw1000_ret_t dw1000_prepare_range_final_msg(dw1000_msg_t* msg);

/*---------------------------------------------------------------------------*/
typedef struct rng_data {
  uint64_t tx_request_ts;
  uint64_t rx_request_ts;
  uint64_t tx_response_ts;
  uint64_t rx_response_ts;
  uint64_t tx_final_ts;
  uint64_t rx_final_ts;
} rng_data_t;

static uint8_t tx_msg_data[127];
static dw1000_msg_t tx_msg = {
  .from_id = 0,
  .to_id = 0,
  .type = 0,
  .data = tx_msg_data,
  .data_length = sizeof(tx_msg_data)
};

/*---------------------------------------------------------------------------*/
static bool final_msg_sent = false;                         // Flag to indicate if the FINAL message has been sent already
static rtimer_clock_t final_msg_rtimer_time = 0;            // Timestamp for the FINAL message  
static uint64_t  rx_response_ts[STAGGERED_RANGING_MAX_IDS]; // Array of the RX timestamps of the responses
static int32_t   R1s_ts[STAGGERED_RANGING_MAX_IDS];         // Array of the R1 intervals (interval between TX of the REQUEST and the RX of the RESPONSE)
static int32_t   D2s_ts[STAGGERED_RANGING_MAX_IDS];         // Array of the D2 intervals (interval between RX of the RESPONSE ant the TX of the FINAL message)
static uint16_t* rng_partner_ids;
static size_t    rng_partner_ids_len;

static rng_data_t rng_data;
static uint8_t response_slot_number = 0;
static bool response_requested = false;
static struct process *dw1000_staggered_range_done_cb = NULL;
static rng_result_t last_rng_result = {0, 0, 0.0f, 0.0f};

static struct pt send_final_pt;
static PT_THREAD(send_final(struct pt* pt));

/*---------------------------------------------------------------------------*/
static void 
send_final_msg(struct rtimer *t, void *ptr)
{
  send_final(&send_final_pt);
}

static void
clear_ranging()
{
  final_msg_sent = false;
  response_slot_number = 0;
  response_requested = false;
  memset((uint8_t*) rx_response_ts, 0, sizeof(rx_response_ts));
  memset((uint8_t*) R1s_ts, 0, sizeof(R1s_ts));
  memset((uint8_t*) D2s_ts, 0, sizeof(D2s_ts));

  rng_data.tx_request_ts = 0;
  rng_data.rx_request_ts = 0;
  rng_data.tx_response_ts = 0;
  rng_data.rx_response_ts = 0;
  rng_data.tx_final_ts = 0;
  rng_data.rx_final_ts = 0;
}

dw1000_ret_t
dw1000_staggered_range_with(uint16_t* ids, size_t n_ids)
{
//  clear_ranging();
 
  if(node_id == 0){
    LOG_WARN("Skip ranging as node_id is 0\n");
    return DW1000_ERR;
  }

  LOG_INFO("initiate staggered ranging with %u ids\n", n_ids);
  if(n_ids > STAGGERED_RANGING_MAX_IDS) {
    LOG_ERR("Too many ids\n");
    return DW1000_ERR;
  }

  rtimer_clock_t max_response_delay = US_TO_RTIMERTICKS(DW1000_ST_RNG_RESPONSE_DELAY_US + (DW1000_ST_RNG_RESPONSE_SLOT_DELAY_US * n_ids));

  rng_partner_ids = ids;
  rng_partner_ids_len = n_ids;

  PT_INIT(&send_final_pt);

  /* Send REQUEST message */
  dw1000_prepare_range_request_msg(ids, n_ids, &tx_msg);
  if(dw1000_send_msg(&tx_msg) != DW1000_OK){
    LOG_ERR("Failed to send request\n");
    return DW1000_ERR;
  }

  /* Wait for transmission to finish */
  while(dw1000_is_transmitting()){}

  rng_data.tx_request_ts = dw1000_get_last_tx_timestamp();

  /* Set timer for final message */
  final_msg_rtimer_time = RTIMER_NOW() + max_response_delay;
  rtimer_set(&dw1000_timer, final_msg_rtimer_time, 0, send_final_msg, NULL);

  return DW1000_OK;
}

dw1000_ret_t
dw1000_staggered_range_set_done_cb(struct process *p)
{
  dw1000_staggered_range_done_cb = p;
  return DW1000_OK;
}

static dw1000_ret_t
dw1000_staggered_range_process_request(const dw1000_msg_t* msg)
{
  LOG_DBG("Processing request\n");
  static volatile uint16_t tx_timeout = 0xFFFF;
  uint16_t* ids = (uint16_t*) msg->data;
  size_t n_ids = msg->data_length / sizeof(uint16_t);
  response_slot_number = 0;
  response_requested = false;

  /* Check if range request is for me and which sub-slot */
  LOG_DBG("Received range request from %u with %u ids\n", msg->from_id, n_ids);
  for (response_slot_number = 0; response_slot_number < n_ids; response_slot_number++){
    LOG_DBG("rng request for %u in slot %d\n", ids[response_slot_number], response_slot_number);
    if (ids[response_slot_number] == node_id){
      LOG_DBG("rng response requested in slot %d\n", response_slot_number);
      response_requested = true;
      break;
    }
  }

  /* If I am not part of the list, return */
  if(!response_requested){
    LOG_DBG("I am not part of the list\n");
    clear_ranging();
    return DW1000_OK;
  }

  /* I am part of the list and a response is requested. Calculate transmission delay */
  rng_data.rx_request_ts = msg->rx_timestamp;

  uint64_t tx_response_ts = (rng_data.rx_request_ts + 
              DW1000_US_TO_RADIO_TIME(DW1000_ST_RNG_RESPONSE_DELAY_US) +
              (DW1000_US_TO_RADIO_TIME(DW1000_ST_RNG_RESPONSE_SLOT_DELAY_US * response_slot_number))) & 0xFFFFFFFFFFLLU;
  uint32_t tx_response_ts_short = ((uint32_t) (tx_response_ts >> 8)) & 0xFFFFFFFEUL; // LSB must be 0 for delayed TX
  dwt_setdelayedtrxtime(tx_response_ts_short);
  rng_data.tx_response_ts = (((uint64_t) tx_response_ts_short) << 8) + DW1000_TX_ANT_DELAY;

  /* Send RESPONSE message */
  dw1000_prepare_range_response_msg(msg->from_id, rng_data.rx_request_ts, 1, response_slot_number, &tx_msg);
  if(dw1000_send_msg(&tx_msg) != DW1000_OK){
    LOG_ERR("Failed to send response\n");
    clear_ranging();
    return DW1000_ERR;
  }

  /* Wait for transmission to finish */
  tx_timeout = 0xFFFF;
  while(dw1000_is_transmitting() && tx_timeout--){}

  if(tx_timeout == 0){
    LOG_ERR("Timeout waiting for transmission to finish\n");
    clear_ranging();
    return DW1000_ERR;
  }

  return DW1000_OK;
}

static dw1000_ret_t
dw1000_staggered_range_process_response(const dw1000_msg_t* msg)
{
  LOG_DBG("Processing response\n");
  volatile uint8_t slot_number = msg->data[0];

  /* Save RX timestamps */
  if(msg->rx_timestamp == 0) LOG_ERR("response message has no rx timestamp\n");
  rx_response_ts[slot_number] = msg->rx_timestamp;

  /* Save R1 timestamps */
  R1s_ts[slot_number] = (int32_t) (msg->rx_timestamp - rng_data.tx_request_ts);

  if(slot_number == rng_partner_ids_len - 1){
    LOG_DBG("recv last response\n");

    send_final(&send_final_pt);
    LOG_DBG("send final after last responder\n");
  }

  return DW1000_OK;
}

static dw1000_ret_t
dw1000_staggered_range_process_final(const dw1000_msg_t* msg)
{
  static uint8_t msg_data[127];
  static int64_t R1, R2, D1, D2;
  static int64_t tof_dtu;
  static double tof, distance_dstwr;
  static uint32_t* data = (uint32_t*) msg_data;
  static const dwt_config_t* cfg;

  LOG_DBG("Processing final\n");

  memcpy((uint8_t*) msg_data, (uint8_t*) msg->data, msg->data_length);

  rng_data.rx_final_ts = msg->rx_timestamp;

  R1 = (int64_t) data[(response_slot_number * 2)];
  R2 = (int64_t) (rng_data.rx_final_ts - rng_data.tx_response_ts);
  D1 = (int64_t) (rng_data.tx_response_ts - rng_data.rx_request_ts);
  D2 = (int64_t) data[(response_slot_number * 2) + 1]; 

  tof_dtu = ( (double)((R1 * R2) - (D1 * D2)) ) / ( (double)(R1 + R2 + D1 + D2) );
  tof = tof_dtu * DWT_TIME_UNITS;
  distance_dstwr = tof * 299702547.0;

  LOG_DBG("Distance       0x%04X <-> 0x%04X, %d\n",node_id, msg->from_id, (int) (distance_dstwr * 100));

#if DW1000_ST_RNG_BIAS_CORRECTION
  cfg = dw1000_get_config();
  distance_dstwr -= dwt_getrangebias(cfg->chan, distance_dstwr, cfg->prf);
  LOG_DBG("Distance(corr) 0x%04X <-> 0x%04X, %d\n",node_id, msg->from_id, (int) (distance_dstwr * 100));
#endif

  // Sanity-checking the timestamps
  if ((R1 == 0) || (R2 == 0) || (D1 == 0) || (D2 == 0)) {
    LOG_WARN("Invalid ranging data R1: %lld, R2: %lld, D1: %lld, D2: %lld\n", R1, R2, D1, D2);
    clear_ranging();
    return DW1000_ERR;
  }
  if(R1 > DW1000_US_TO_RADIO_TIME(((uint64_t) DW1000_ST_RNG_SLOT_DURATION_US))){
    LOG_DBG("R1 too large: %lu us\n", (uint32_t) DW1000_RADIO_TIME_TO_US(R2));
    clear_ranging();
    return DW1000_ERR;
  }
  if(R2 > DW1000_US_TO_RADIO_TIME(((uint64_t) DW1000_ST_RNG_SLOT_DURATION_US))){
    LOG_DBG("R2 too large: %lu us\n", (uint32_t) DW1000_RADIO_TIME_TO_US(R2));
    clear_ranging();
    return DW1000_ERR;
  }
  if(D1 > DW1000_US_TO_RADIO_TIME(((uint64_t) DW1000_ST_RNG_SLOT_DURATION_US))){
    LOG_DBG("D1 too large: %lu us\n", (uint32_t) DW1000_RADIO_TIME_TO_US(D1));
    clear_ranging();
    return DW1000_ERR;
  }
  if(D2 > DW1000_US_TO_RADIO_TIME(((uint64_t) DW1000_ST_RNG_SLOT_DURATION_US))){
    LOG_DBG("D2 too large: %lu us\n", (uint32_t) DW1000_RADIO_TIME_TO_US(D2));
    clear_ranging();
    return DW1000_ERR;
  }
  if(R1 - D1 > DW1000_US_TO_RADIO_TIME(10000)){ // R1 should be close to D1 as they both include the same RX response delay
    LOG_DBG("R1 and D1 differ too much: R1: %lu us, D1: %lu us\n", (uint32_t) DW1000_RADIO_TIME_TO_US(R1), (uint32_t) DW1000_RADIO_TIME_TO_US(D1));
    clear_ranging();
    return DW1000_ERR;
  }
  if(R2 - D2 > DW1000_US_TO_RADIO_TIME(10000)){ // R2 should be close to D2 as they both include the same TX response delay
    LOG_DBG("R2 and D2 differ too much: R2: %lu us, D2: %lu us\n", (uint32_t) DW1000_RADIO_TIME_TO_US(R2), (uint32_t) DW1000_RADIO_TIME_TO_US(D2));
    clear_ranging();
    return DW1000_ERR;
  }

  if(distance_dstwr < 0 || distance_dstwr > 30.0){
    LOG_ERR("Unrealistic distance: %lu cm\n", (uint32_t) (distance_dstwr * 100));
    LOG_ERR("R1: %lld, R2: %lld, D1: %lld, D2: %lld\n", R1, R2, D1, D2);
    clear_ranging();
    return DW1000_ERR;
  }

  /* Calculate SS-TWR distance for the lulz */
  double tof_sstwr = (R1 - D1) / 2.0;
  double distance_sstwr = tof_sstwr * DWT_TIME_UNITS * 299702547.0;
  distance_sstwr -= dwt_getrangebias(dw1000_get_config()->chan, distance_sstwr, dw1000_get_config()->prf);
  LOG_DBG("Distance SSTWR: %lu cm, CFO_PPM: %ld\n", (int32_t) (distance_sstwr * 100), (int32_t) (msg->rx_cfo_ppm));

  double tof_sstwr_corr = (R1 - (D1 * (1.0 - (msg->rx_cfo_ppm / 1e6)))) / 2.0;
  double dist_sstwr_corr = tof_sstwr_corr * DWT_TIME_UNITS * 299702547.0;
  dist_sstwr_corr -= dwt_getrangebias(dw1000_get_config()->chan, dist_sstwr_corr, dw1000_get_config()->prf);

  /* Save ranging results in nbr-table */
  dw1000_nbr_table_update(msg->from_id, (int16_t) (distance_dstwr * 1000));

  if(dw1000_staggered_range_done_cb != NULL) {
    last_rng_result.from = msg->from_id;
    last_rng_result.to = node_id;
    last_rng_result.dist_dstwr_mm = (float) distance_dstwr * 1000.0f;
    last_rng_result.dist_sstwr_mm = (float) distance_sstwr * 1000.0f;
    last_rng_result.dist_sstwr_corr_mm = (float) dist_sstwr_corr * 1000.0f;
    last_rng_result.slot_number = response_slot_number;
    last_rng_result.cfo_ppm = msg->rx_cfo_ppm;
    process_post(dw1000_staggered_range_done_cb, PROCESS_EVENT_POLL, &last_rng_result);
  } else {
    LOG_DBG("No done callback set, cannot post result\n");
  }

  clear_ranging();
  return DW1000_OK;
}

static dw1000_ret_t 
dw1000_prepare_range_request_msg(uint16_t* ids, size_t n_ids, dw1000_msg_t* msg)
{
  LOG_DBG("preparing range request msg\n");
  msg->from_id = node_id;
  msg->to_id = 0xFFFF;  // Broadcast
  msg->type = ST_RNG_REQUEST;
  msg->data_length = sizeof(uint16_t) * n_ids;;
  msg->tx_timestamp = 0;
  memcpy((uint8_t*) tx_msg_data, (uint8_t*) ids, n_ids * sizeof(uint16_t));

  return DW1000_OK;
}

static dw1000_ret_t
dw1000_prepare_range_response_msg(uint16_t to, uint64_t rx_request_ts, uint64_t tx_response_ts, uint8_t slot_number, dw1000_msg_t* msg){
  tx_msg_data[0] = slot_number;

  msg->from_id = node_id;
  msg->to_id = to;
  msg->type = ST_RNG_RESPONSE;
  msg->data = tx_msg_data;
  msg->data_length = 1;
  msg->tx_timestamp = rng_data.tx_response_ts;

  return DW1000_OK;
}

static dw1000_ret_t 
dw1000_prepare_range_final_msg(dw1000_msg_t* msg)
{
  if((sizeof(uint32_t) * rng_partner_ids_len * 2) > 120) {
    LOG_ERR("Final msg too long\n");
    return DW1000_ERR;
  }

  uint64_t cur_ts = ((uint64_t) dwt_readsystimestamphi32()) << 8;
  uint64_t tx_final_ts = (cur_ts + DW1000_US_TO_RADIO_TIME(DW1000_ST_RNG_FINAL_DELAY_US)) & 0xFFFFFFFFFFLLU;
  uint32_t tx_final_ts_short = ((uint32_t) (tx_final_ts >> 8)) & 0xFFFFFFFEUL; // LSB must be 0 for delayed TX
  dwt_setdelayedtrxtime(tx_final_ts_short);
  rng_data.tx_final_ts = (((uint64_t) tx_final_ts_short) << 8) + DW1000_TX_ANT_DELAY;

  /* Calculate D2s */
  for(size_t i = 0; i < rng_partner_ids_len; i++){
    if(rx_response_ts[i] != 0){
      int32_t D2 = (rng_data.tx_final_ts - rx_response_ts[i]);
      D2s_ts[i] = D2;
    }else{
      D2s_ts[i] = 0;
    }
  }

  uint32_t* data = (uint32_t*) tx_msg_data;
  for(size_t i = 0; i < rng_partner_ids_len; i++){
    int32_t R1 = (int32_t) (R1s_ts[i]);
    int32_t D2 = (int32_t) (D2s_ts[i]);
    if(R1 == 0) LOG_DBG("R1 for partner %u is 0\n", rng_partner_ids[i]);
    if(D2 == 0) LOG_DBG("D2 for partner %u is 0\n", rng_partner_ids[i]);
    data[(i*2)] = R1;     // R1
    data[(i*2) + 1] = D2; // D2
  }


  msg->from_id = node_id;
  msg->to_id = 0xFFFF; // Broadcast
  msg->type = ST_RNG_FINAL;
  msg->data = tx_msg_data;
  msg->tx_timestamp = rng_data.tx_final_ts;
  msg->data_length = (sizeof(uint32_t) * rng_partner_ids_len * 2);

  return DW1000_OK;
}

static
PT_THREAD(send_final(struct pt* pt))
{
  PT_BEGIN(pt);

  if(final_msg_sent) PT_EXIT(pt);
  final_msg_sent = true;
  dw1000_prepare_range_final_msg(&tx_msg);
  if(dw1000_send_msg(&tx_msg) != DW1000_OK){
    LOG_ERR("Failed to send final message\n");
    clear_ranging();
    PT_EXIT(pt);
  }
  LOG_DBG("send final message after timeout\n");  
  PT_END(pt);
}


PT_THREAD(dw1000_staggered_range_process(struct pt* pt))
{
  PT_BEGIN(pt);
  static const dw1000_msg_t* last_msg;

  last_msg = dw1000_get_msg();

  if(last_msg-> type == ST_RNG_REQUEST){
    LOG_DBG("RNG REQUEST\n");
    dw1000_staggered_range_process_request(last_msg);
    PT_EXIT(pt);
  }else if(last_msg-> type == ST_RNG_RESPONSE){
    LOG_DBG("RNG RESPONSE\n");
    dw1000_staggered_range_process_response(last_msg);
    PT_EXIT(pt);
  }else if(last_msg-> type == ST_RNG_FINAL){
    LOG_DBG("RNG FINAL\n");
    dw1000_staggered_range_process_final(last_msg);
    PT_EXIT(pt);
  }else{
    LOG_WARN("Unknown message type 0x%02X\n", last_msg->type);
  }
  PT_END(pt);
}
