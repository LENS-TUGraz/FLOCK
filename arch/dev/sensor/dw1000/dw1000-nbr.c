#include "dw1000-nbr.h"

#include "stdint.h"
#include "stddef.h"
#include "stdbool.h"
#include "os/lib/random.h"

#include "sys/log.h"
#define LOG_MODULE "DW1000 NBR"
#define LOG_LEVEL LOG_LEVEL_INFO

static dw1000_nbr_t dw1000_nbrs[DW1000_NBR_MAX];
static size_t dw1000_nbr_table_len = 0;
static size_t dw1000_nbr_table_max_len = DW1000_NBR_MAX;

void dw1000_nbr_table_init()
{
  dw1000_nbr_table_len = 0;
  dw1000_nbr_table_max_len = DW1000_NBR_MAX;
  dw1000_nbr_table_clear();
}

void dw1000_nbr_table_update(uint16_t id, uint16_t dist_mm)
{
  for (size_t i = 0; i < DW1000_NBR_MAX; i++){
    /* update existing value */
    if ((dw1000_nbrs[i].id == id)){
      if(dist_mm != 0){
        dw1000_nbrs[i].dist_mm = dist_mm;
        dw1000_nbrs[i].last_update = RTIMER_NOW();;
      }
      dw1000_nbrs[i].is_valid = true;
      return;
    }
  }
  /* add new value */
  for (size_t i = 0; i < DW1000_NBR_MAX; i++){
    if (dw1000_nbrs[i].is_valid == false){
      dw1000_nbrs[i].id = id;
      dw1000_nbrs[i].dist_mm = dist_mm;
      dw1000_nbrs[i].last_update = RTIMER_NOW();
      dw1000_nbrs[i].is_valid = true;
      dw1000_nbr_table_len++;
      return;
    }
  }
}

void dw1000_nbr_table_remove(uint16_t id)
{
  for (size_t i = 0; i < DW1000_NBR_MAX; i++){
    if (dw1000_nbrs[i].id == id){
      dw1000_nbrs[i].is_valid = false;
      dw1000_nbr_table_len--;
      return;
    }
  }
}

void dw1000_nbr_table_print()
{
  LOG_INFO("[");
  bool first = true;
  for (size_t i = 0; i < DW1000_NBR_MAX; i++){
    if (dw1000_nbrs[i].is_valid){
      if (!first) {
        LOG_INFO_(",");
      }
      LOG_INFO_("{\"id\":%u,\"dist_mm\":%d,\"last_update\":%lu}", 
             dw1000_nbrs[i].id, 
             dw1000_nbrs[i].dist_mm, 
             dw1000_nbrs[i].last_update);
      first = false;
    }
  }
  LOG_INFO_("]\n");
}

void dw1000_nbr_table_clear()
{
  for (size_t i = 0; i < DW1000_NBR_MAX; i++){
    dw1000_nbrs[i].id = 0;
    dw1000_nbrs[i].dist_mm = 0;
    dw1000_nbrs[i].last_update = 0;
    dw1000_nbrs[i].is_nlos = false;
    dw1000_nbrs[i].is_valid = false;
  }
  dw1000_nbr_table_len = 0;
}

size_t dw1000_nbr_table_get_length()
{
  return dw1000_nbr_table_len;
}

const dw1000_nbr_t*
dw1000_nbr_table_get(uint16_t id)
{
  for (size_t i = 0; i < DW1000_NBR_MAX; i++){
    if (dw1000_nbrs[i].id == id){
      return &dw1000_nbrs[i];
    }
  }
  return NULL;
}

dw1000_nbr_t *dw1000_nbr_table_get_elem(size_t i)
{
  if (i < DW1000_NBR_MAX){
    return &dw1000_nbrs[i];
  }
  return NULL;
}

void dw1000_nbr_table_prune(rtimer_clock_t rtimer_threshold)
{
  static rtimer_clock_t now;
  static size_t i;

  now = RTIMER_NOW();
  for (i = 0; i < DW1000_NBR_MAX; i++){
    if (dw1000_nbrs[i].is_valid && 
        (RTIMER_CLOCK_DIFF(now, dw1000_nbrs[i].last_update) > rtimer_threshold)) {
      LOG_ERR("Pruning neighbor id %u, last update %lu\n", 
               dw1000_nbrs[i].id, dw1000_nbrs[i].last_update);
      dw1000_nbrs[i].is_valid = false;
      dw1000_nbr_table_len--;
    }
  }
}