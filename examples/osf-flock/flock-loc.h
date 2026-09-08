#ifndef FLOCK_LOC_H
#define FLOCK_LOC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "flock-pos-table.h"

extern uint32_t flock_loc_iterations;
extern float flock_loc_learning_rate;

PROCESS_NAME(flock_loc_process);

typedef struct {
  uint16_t id;
  double x;
  double y;
  flock_pos_type_t type; // Type of position
  uint16_t error_mm;
} node_position_t;

typedef struct {
  size_t num_iterations;
  float learning_rate;
  uint16_t average_error_mm;
  uint8_t retries;
  uint32_t runtime_us;                // Runtime of the localization in microseconds
  uint32_t runtime_iteration_us;      // Runtime of the last iteration in microseconds
} flock_loc_diagnostics_t;


bool flock_loc_estimate_positions(flock_pos_table_t* pos_table);
void flock_loc_set_iterations(uint32_t iterations);
void flock_loc_set_learning_rate(float lr);
const flock_loc_diagnostics_t* flock_loc_get_diagnostics();
void flock_loc_print_diagnostics(const flock_loc_diagnostics_t* diagnostics);
void flock_loc_print_result(rtimer_clock_t rtime_start, rtimer_clock_t rtime_end, bool success);

#endif