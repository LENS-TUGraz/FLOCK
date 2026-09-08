#ifndef _FLOCK_H_
#define _FLOCK_H_

#include "contiki.h"
#include "flock-pos-table.h"

PROCESS_NAME(flock_process);

extern flock_pos_table_t flock_pos_table;

void flock_osf_input_callback(uint8_t src, uint8_t dest, uint8_t *data, uint8_t len);
void flock_osf_sync_callback();

#endif /* _FLOCK_H_ */