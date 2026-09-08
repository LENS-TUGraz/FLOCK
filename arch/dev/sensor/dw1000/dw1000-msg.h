#ifndef _DW1000_MSG_H_
#define _DW1000_MSG_H_

#include "contiki.h"

#include "dw1000.h"
#include "dw1000-types.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef enum {
  ST_RNG_REQUEST = 0x11,
  ST_RNG_RESPONSE = 0x12,
  ST_RNG_FINAL = 0x13
} dw1000_msg_type_t;

typedef struct dw1000_msg {
  uint16_t from_id;
  uint16_t to_id;
  dw1000_msg_type_t type;
  uint8_t *data;
  size_t data_length;
  uint64_t rx_timestamp;
  uint64_t tx_timestamp;
  float rx_cfo_ppm;
} dw1000_msg_t;

/* 
 * DW1000 message format:
 * +----------------+----------------+----------------+--------------+--------------+
 * | from_id (2B)   | to_id (2B)     |  type (1B)     | data (?B)    | CRC (2B)     |
 * +----------------+----------------+----------------+--------------+--------------+
 * 
*/
#define DW1000_MSG_FROM_ID_SIZE 2
#define DW1000_MSG_TO_ID_SIZE 2
#define DW1000_MSG_TYPE_SIZE 1
#define DW1000_MSG_HEADER_SIZE (DW1000_MSG_FROM_ID_SIZE + DW1000_MSG_TO_ID_SIZE + DW1000_MSG_TYPE_SIZE)

#define DW1000_MSG_FROM_ID_OFFSET 0
#define DW1000_MSG_TO_ID_OFFSET (DW1000_MSG_FROM_ID_OFFSET + DW1000_MSG_FROM_ID_SIZE)
#define DW1000_MSG_TYPE_OFFSET (DW1000_MSG_TO_ID_OFFSET + DW1000_MSG_TO_ID_SIZE)
#define DW1000_MSG_DATA_OFFSET (DW1000_MSG_TYPE_OFFSET + DW1000_MSG_TYPE_SIZE)


dw1000_ret_t dw1000_parse_msg(const uint8_t* rx_buf, size_t length);
dw1000_ret_t dw1000_prepare_msg(const dw1000_msg_t* msg, uint8_t* tx_buffer);
dw1000_ret_t dw1000_send_msg(dw1000_msg_t* msg);
dw1000_msg_t* dw1000_get_msg();

#endif /* _DW1000_MSG_H_ */