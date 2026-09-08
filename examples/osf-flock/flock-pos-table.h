#ifndef _FLOCK_POS_TABLE_H_
#define _FLOCK_POS_TABLE_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "sys/rtimer.h"
#include "os/net/mac/osf/osf.h"

#include "flock-conf.h"

typedef enum {
  NONE = 0,
  ESTIMATE, // Estimated positions will be taken as initial guess for localization engine but dont contribute to the gradient
  UNKNOWN,  // Unknown positions will be ignored by the localization engine
  KNOWN,    // Known positions will be taken as initial guess for localization engine and contribute to the gradient
  FIXED     // Fixed positions will be taken as is and will not be updated
} flock_pos_type_t;

typedef struct {
  uint16_t id;              // Node ID
  float x;                  // X coordinate in mm
  float y;                  // Y coordinate in mm
  bool valid;               // True if position is valid
  uint16_t error_mm;        // Estimated error in mm
  flock_pos_type_t type;    // Type of position
  uint16_t osf_epoch;       // OSF epoch when the position was last updated
} flock_pos_entry_t;

typedef struct {
  flock_pos_entry_t entries[FLOCK_POS_TABLE_SIZE];
  size_t length;     // Current number of entries
  size_t max_length; // Maximum number of entries
} flock_pos_table_t;

void flock_pos_table_init(flock_pos_table_t *table);
int flock_pos_table_add(flock_pos_table_t *table, uint16_t id, float x, float y, flock_pos_type_t type, uint16_t osf_epoch, uint16_t error_mm);
const flock_pos_entry_t* flock_pos_table_get(const flock_pos_table_t *table, uint16_t id);
const flock_pos_entry_t* flock_pos_table_get_head(const flock_pos_table_t *table);
const flock_pos_entry_t* flock_pos_table_get_next(const flock_pos_table_t *table, const flock_pos_entry_t *current);
void flock_pos_table_remove(flock_pos_table_t *table, uint16_t id);
void flock_pos_table_print_json(const flock_pos_table_t *table);
PT_THREAD(flock_pos_table_prune(struct pt* pt, flock_pos_table_t *table, uint16_t epochs));

#endif