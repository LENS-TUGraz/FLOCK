#ifndef _DW1000_NBR_H_
#define _DW1000_NBR_H_

#include "contiki.h"
#include "dw1000-conf.h"
#include "dw1000-types.h"
#include "stdbool.h"
#include "stdint.h"

#define DW1000_NBR_MAX DW1000_ST_RNG_MAX_NBR

typedef struct dw1000_nbr{
  uint16_t id;
  uint16_t dist_mm;
  uint16_t dist_mm_sstwr;
  rtimer_clock_t last_update;
  float rx_cfo_ppm;
  bool is_nlos;
  bool is_valid;
} dw1000_nbr_t;

void dw1000_nbr_table_init();
void dw1000_nbr_table_update(uint16_t id, uint16_t dist_mm);
void dw1000_nbr_table_remove(uint16_t id);
void dw1000_nbr_table_print();
void dw1000_nbr_table_clear();
size_t dw1000_nbr_table_get_length();
const dw1000_nbr_t *dw1000_nbr_table_get(uint16_t id);
dw1000_nbr_t *dw1000_nbr_table_get_elem(size_t i);
void dw1000_nbr_table_prune(rtimer_clock_t rtimer_threshold);




#endif /* _DW1000_NBR_H_ */