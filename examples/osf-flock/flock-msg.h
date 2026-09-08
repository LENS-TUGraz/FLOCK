#ifndef _FLOCK_MSG_H_
#define _FLOCK_MSG_H_

#include "contiki.h"
#include "osf.h"

#include <stdint.h>
#include <stddef.h>

enum flock_msg_type {
  FLOCK_REPORT = 0x4242,
};

typedef struct flock_msg {
  uint16_t from;
  uint16_t to;
  uint16_t type;
  uint8_t* data;
  size_t data_len;
} flock_msg_t;

#define FLOCK_MSG_TYPE_SIZE sizeof(uint16_t)
#define FLOCK_MSG_TYPE_OFFSET 0
#define FLOCK_MSG_DATA_OFFSET (FLOCK_MSG_TYPE_OFFSET + FLOCK_MSG_TYPE_SIZE)

PROCESS_NAME(flock_message_process);


void flock_msg_send(uint16_t from, uint16_t to, uint16_t type, uint8_t* data, size_t data_len);

#endif /* _FLOCK_MSG_H_ */