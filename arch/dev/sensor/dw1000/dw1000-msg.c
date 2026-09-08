#include "dw1000-msg.h"
#include "dw1000.h"
#include "dw1000-nbr.h"
#include "dw1000-conf.h"
#include "dw1000-staggered-ranging.h"
#include "sys/node-id.h"
#include <string.h>

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "DW1000 MSG"
#define LOG_LEVEL LOG_LEVEL_INFO

static uint8_t tx_buffer[DW1000_BUFFER_SIZE]; // TX buffer

struct pt staggered_ranging_pt;

static uint8_t msg_data[128];
static dw1000_msg_t msg = {
  .from_id = 0,
  .to_id = 0,
  .type = 0,
  .data = msg_data,
  .data_length = sizeof(msg_data)
};


dw1000_ret_t dw1000_parse_msg(const uint8_t* rx_buf, size_t length)
{
  LOG_DBG("parse msg\n");
  msg.from_id = rx_buf[0] | (rx_buf[1] << 8);
  msg.to_id = rx_buf[2] | (rx_buf[3] << 8);
  msg.type = rx_buf[4];
  msg.data_length = length - DW1000_MSG_HEADER_SIZE - 2;
  msg.rx_timestamp = dw1000_get_last_rx_timestamp();
  msg.rx_cfo_ppm = dw1000_get_last_rx_cfo_ppm();

  /* update neighbour table */
  if(msg.from_id != 0 && msg.from_id != 0xFF) dw1000_nbr_table_update(msg.from_id, 0);

  /* check if message is for me or a broadcast */
  if ( (msg.to_id != node_id) && (msg.to_id != 0xFFFF)) {
    LOG_DBG("Frame not for me but for 0x%04x\n", msg.to_id);
    return DW1000_ERR;
  }

  /* copy msg.data from rx_buf */
  memcpy((uint8_t*) (msg.data), (uint8_t*) &rx_buf[DW1000_MSG_DATA_OFFSET], length - DW1000_MSG_DATA_OFFSET);

  if(msg.type & (ST_RNG_REQUEST | ST_RNG_RESPONSE | ST_RNG_FINAL)){
    LOG_DBG("parsed staggered ranging message\n");
    dw1000_staggered_range_process(&staggered_ranging_pt);
  }else{
    LOG_ERR("Unknown message type 0x%02X\n", msg.type);
    return DW1000_ERR;
  }

  return DW1000_OK;
}

dw1000_ret_t 
dw1000_prepare_msg(const dw1000_msg_t* msg, uint8_t* tx_buffer)
{

  /* build tx frame */
  memcpy((uint8_t*) &tx_buffer[DW1000_MSG_FROM_ID_OFFSET], (uint8_t*) &msg->from_id, DW1000_MSG_FROM_ID_SIZE);
  memcpy((uint8_t*) &tx_buffer[DW1000_MSG_TO_ID_OFFSET],   (uint8_t*) &msg->to_id,   DW1000_MSG_TO_ID_SIZE);
  memcpy((uint8_t*) &tx_buffer[DW1000_MSG_TYPE_OFFSET],    (uint8_t*) &msg->type,    DW1000_MSG_TYPE_SIZE);
  if(msg->data_length > 0)
    memcpy((uint8_t*) &tx_buffer[DW1000_MSG_DATA_OFFSET],    (uint8_t*) msg->data,    msg->data_length);

  return DW1000_OK;
}

dw1000_ret_t
dw1000_send_msg(dw1000_msg_t* msg)
{
  size_t tx_frame_length;
  uint8_t tx_mode = 0;

  LOG_DBG("send msg\n");

  /* prepare message */
  dw1000_prepare_msg(msg, tx_buffer);

  dw1000_off();

  /* prepare radio */
  tx_frame_length = DW1000_MSG_FROM_ID_SIZE + DW1000_MSG_TO_ID_SIZE + DW1000_MSG_TYPE_SIZE + msg->data_length;

  if(msg->tx_timestamp != 0){
    tx_mode |= DWT_START_TX_DELAYED;
  }

  /* start transmission */
  if(dw1000_send(tx_buffer, tx_frame_length, tx_mode) != DW1000_OK){
    uint32_t cur_us = DW1000_RADIO_TIME_TO_US( ((uint64_t) dwt_readsystimestamphi32() << 8) );
    uint32_t last_rx_us = DW1000_RADIO_TIME_TO_US(dw1000_get_last_rx_timestamp());
    uint32_t delayed_tx_us = DW1000_RADIO_TIME_TO_US(msg->tx_timestamp);

    LOG_DBG("delayed tx failed\n");
    LOG_DBG("last rx ts   : %lu us\n", last_rx_us);
    LOG_DBG("delayed tx ts: %lu us\n", delayed_tx_us);
    LOG_DBG("cur ts       : %lu us\n", cur_us);
    return DW1000_ERR;
  }

  msg->tx_timestamp = 0;
  LOG_DBG("send msg done\n");
  return DW1000_OK;
}

dw1000_msg_t* 
dw1000_get_msg()
{
  return &msg;
}