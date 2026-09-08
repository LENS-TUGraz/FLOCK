#include "flock.h"
#include "flock-conf.h"
#include "flock-loc.h"
#include "flock-dist-table.h"
#include "flock-pos-table.h"
#include "osf.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <float.h>

#include "lib/random.h"
#include "sys/node-id.h"

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "FLOCK LOC"
#define LOG_LEVEL LOG_LEVEL_WARN

uint32_t flock_loc_iterations = FLOCK_LOC_ITERATIONS;
float flock_loc_learning_rate = FLOCK_LOC_LEARNING_RATE;
flock_loc_diagnostics_t flock_loc_diagnostics;

PROCESS(flock_loc_process, "Flock Position Calculation Process");

static size_t collect_unique_ids(uint16_t *ids, size_t max_ids) {
  size_t count = 0;
  const flock_dist_table_element_t *el = flock_dist_table_get_head();
  while(el) {
    int found_from = 0, found_to = 0;
    for(size_t i=0; i<count; i++) {
      if(ids[i] == el->from_id) found_from = 1;
      if(ids[i] == el->to_id) found_to = 1;
    }
    if(!found_from && count < max_ids) ids[count++] = el->from_id;
    if(!found_to && count < max_ids) ids[count++] = el->to_id;
    el = flock_dist_table_get_next(el);
  }
  return count;
}

/* Convert id to index */
static size_t id_index(uint16_t *ids, size_t n, uint16_t id) {
  for(size_t i=0; i<n; i++) if(ids[i] == id) return i;
  return -1;
}

/* Initialize positions from the position table
  FIXED, KNOWN or ESTIMATE positions are taken from the position table.
  1. ESTIMATE (lowest priority)
  2. KNOWN
  3. FIXED    (highest priority)
*/
static void init_positions(node_position_t* positions, flock_pos_table_t* pos_table, uint16_t* ids, size_t n) {
  static const flock_pos_entry_t* pos_elem;

  /* Reset positions */
  for(size_t i=0; i<FLOCK_POS_TABLE_SIZE; i++) {
    positions[i].id = 0;
    positions[i].x = 0.0;
    positions[i].y = 0.0;
    positions[i].type = NONE;
  }

  pos_elem = flock_pos_table_get_head(pos_table);
  for (pos_elem = flock_pos_table_get_head(pos_table); pos_elem != NULL; pos_elem = flock_pos_table_get_next(pos_table, pos_elem)) {
    size_t index = id_index(ids, n, pos_elem->id);
    if(index != (size_t)-1) {
      if(positions[index].type == FIXED) {
        continue; // FIXED has highest priority
      }
      if(positions[index].type == KNOWN) {
        continue; // KNOWN has higher priority than ESTIMATE
      }

      if (positions[index].error_mm > FLOCK_LOC_APRIORI_RANGE_ERROR_MM){
        positions[index].type = NONE; // If the estimated error is higher than the apriori error, ignore the position
        continue;
      }

      positions[index].id = pos_elem->id;
      positions[index].x = pos_elem->x / 1000.0f;
      positions[index].y = pos_elem->y / 1000.0f;
      positions[index].type = pos_elem->type;
    }
  }
  // if there are no given positions in the pos table, initialize them randomly
  static float max_x_m = FLOCK_LOC_INIT_AREA_M / 2.0f;
  static float max_y_m = FLOCK_LOC_INIT_AREA_M / 2.0f;
  static float min_x_m = -FLOCK_LOC_INIT_AREA_M / 2.0f;;
  static float min_y_m = -FLOCK_LOC_INIT_AREA_M / 2.0f;

  for(size_t i=0; i<n; i++) {
    if(positions[i].type == NONE) {
      positions[i].type = UNKNOWN;
      positions[i].id = ids[i];
      positions[i].x = min_x_m + (float)(random_rand() % (int)(max_x_m - min_x_m));
      positions[i].y = min_y_m + (float)(random_rand() % (int)(max_y_m - min_y_m));
    }
  }
}

static void gradient_descent_run(node_position_t* positions, flock_pos_table_t* pos_table, uint16_t* ids, size_t n){
  // Gradient descent
  size_t iter;
  uint16_t avg_error_mm, avg_error_mm_prev;
  float deg_freedom;
  static uint8_t early_break_counter;

  flock_loc_diagnostics.num_iterations = 0;
  flock_loc_diagnostics.learning_rate = flock_loc_learning_rate;
  flock_loc_diagnostics.average_error_mm = 0;
  avg_error_mm = 0;
  avg_error_mm_prev = UINT16_MAX;
  early_break_counter = 0;

  // Berechnung der Freiheitsgrade 
  //deg_freedom = (float) (flock_dist_table_length() + pos_table->length - (2*n)); // TODO: check if this code makes sense
  deg_freedom = (float) (flock_dist_table_length() - (2*n));

  for(iter=0; iter<flock_loc_iterations; iter++) {
    float grad_x[FLOCK_POS_TABLE_SIZE] = {0}, grad_y[FLOCK_POS_TABLE_SIZE] = {0};
    const flock_dist_table_element_t *el = flock_dist_table_get_head();
    const flock_pos_entry_t *pos_elem = flock_pos_table_get_head(pos_table);
    float e_2 = 0.0f;

    // Check distance measurements
    while(el) {
      size_t i = id_index(ids, n, el->from_id);
      size_t j = id_index(ids, n, el->to_id);
      if((i >= 0) && (j >= 0) && (el->is_valid) && (i != j) && (el->dist_mm > FLOCK_LOC_MIN_DIST_MM) && (el->dist_mm < FLOCK_LOC_MAX_DIST_MM)) {
        // Calculate gradient for the distance constraint
        float dx = positions[i].x - positions[j].x;
        float dy = positions[i].y - positions[j].y;
        float dist = sqrtf(dx*dx + dy*dy);
        float error = dist - ((float)el->dist_mm / 1000.0f);
        e_2 += error * error;
        
        float grad = (error / dist);
        grad_x[i] += (grad * dx);
        grad_y[i] += (grad * dy);
        grad_x[j] -= (grad * dx);
        grad_y[j] -= (grad * dy);
      }
      el = flock_dist_table_get_next(el);
    }


    // Check known positions
    while(pos_elem) {
      size_t i = id_index(ids, n, pos_elem->id);
      if((i >= 0) && (pos_elem->valid) &&
         ((pos_elem->type == KNOWN) || (pos_elem->type == FIXED))) {
        // Calculate gradient for the known position constraint
        float dx = positions[i].x - (pos_elem->x / 1000.0f);
        float dy = positions[i].y - (pos_elem->y / 1000.0f);

        if(dx == 0.0f && dy == 0.0f) {
          pos_elem = flock_pos_table_get_next(pos_table, pos_elem);
          continue;
        }
        float error = sqrtf(dx*dx + dy*dy); // euclidean distance between the estimated and known postition
//        e_2 += error * error;

        float grad = error;
        grad_x[i] += (grad * dx);
        grad_y[i] += (grad * dy);
      }
      pos_elem = flock_pos_table_get_next(pos_table, pos_elem);
    }

    // Update positions
    for(size_t i=0; i<n; i++) {
      if(positions[i].type != FIXED) { // Only update non-fixed positions
        // Apply gradient descent step
        positions[i].x -= flock_loc_learning_rate * grad_x[i];
        positions[i].y -= flock_loc_learning_rate * grad_y[i];
      }
    }

    // Calculate average error
    avg_error_mm_prev = avg_error_mm;
    avg_error_mm = (uint16_t) (sqrtf(e_2 / deg_freedom) * 1000);
#if FLOCK_LOC_EARLY_BREAK_THRESHOLD_MM > 0
    // Check for early break
    if ( (iter > 0) && ((avg_error_mm_prev - avg_error_mm) < FLOCK_LOC_EARLY_BREAK_THRESHOLD_MM)) {
      if(early_break_counter++ >= FLOCK_LOC_EARLY_BREAK_ITERS) {
        LOG_INFO("Early break condition met %d times\n", early_break_counter);
        LOG_INFO("Early break at iteration %d, average error mm: %d\n", iter, avg_error_mm);
        break;
      }
//      LOG_INFO("Early break at iteration %d, average error mm: %d\n", iter, avg_error_mm);
//      break;
    }else{
      early_break_counter = 0;
    }
#endif
    LOG_INFO("iteration: %d, average_error_mm : %d\n", iter,  avg_error_mm);
  }
  flock_loc_diagnostics.num_iterations = iter;
  flock_loc_diagnostics.average_error_mm = avg_error_mm;
}

bool flock_loc_estimate_positions(flock_pos_table_t* pos_table) {
  static rtimer_clock_t start_time_total, end_time_total;
  static rtimer_clock_t start_time_run, end_time_run;
  static node_position_t positions[FLOCK_POS_TABLE_SIZE];
  static node_position_t positions_best[FLOCK_POS_TABLE_SIZE];
  static flock_loc_diagnostics_t flock_loc_diagnostics_best;
  static uint16_t ids[FLOCK_POS_TABLE_SIZE];
  static size_t retry;
  static bool confident = false;

  confident = false;
  flock_loc_diagnostics.num_iterations = 0;
  flock_loc_diagnostics.learning_rate = flock_loc_learning_rate;
  flock_loc_diagnostics.average_error_mm = 0;
  flock_loc_diagnostics_best.num_iterations = 0;
  flock_loc_diagnostics_best.learning_rate = flock_loc_learning_rate;
  flock_loc_diagnostics_best.average_error_mm = UINT16_MAX;

  /* Prune old entries from distance and position tables */
  static struct pt prune_pt;
  PT_INIT(&prune_pt);
  while(flock_dist_table_prune(&prune_pt, FLOCK_LOC_RNG_EPOCHS_TO_KEEP) < PT_EXITED);
  while(flock_pos_table_prune(&prune_pt, pos_table, FLOCK_LOC_POS_EPOCHS_TO_KEEP) < PT_EXITED);

  // Collect unique IDs from the distance table
  size_t n = collect_unique_ids(ids, FLOCK_POS_TABLE_SIZE);
  LOG_DBG("Found %zu unique IDs in the distance table\n", n);

  start_time_total = RTIMER_NOW();

  for(retry = 0; retry < FLOCK_LOC_MAX_RETRIES; retry++) {
    // Initialize positions from the position table
    init_positions(positions, pos_table, ids, n);

    // Run gradient descent
    start_time_run = RTIMER_NOW();
    gradient_descent_run(positions, pos_table, ids, n);
    end_time_run = RTIMER_NOW();

    flock_loc_diagnostics.runtime_iteration_us = (uint32_t) RTIMERTICKS_TO_US_64(RTIMER_CLOCK_DIFF(end_time_run, start_time_run));

    // Check if localization was successful
    if(flock_loc_diagnostics.average_error_mm > (FLOCK_LOC_APRIORI_RANGE_ERROR_MM) ||
        flock_loc_diagnostics.average_error_mm == 0) {
      LOG_DBG("Localization failed, average error mm: %d after %d iterations %d retries\n", flock_loc_diagnostics.average_error_mm, flock_loc_diagnostics.num_iterations, retry);
    }else{
      confident = true;
      break; // Successful localization
    }

    // Track best result
    if(flock_loc_diagnostics.average_error_mm > 0 && 
       flock_loc_diagnostics.average_error_mm < flock_loc_diagnostics_best.average_error_mm) {
      flock_loc_diagnostics_best.average_error_mm = flock_loc_diagnostics.average_error_mm;
      memcpy(&flock_loc_diagnostics_best, &flock_loc_diagnostics, sizeof(flock_loc_diagnostics));
      memcpy(positions_best, positions, sizeof(positions));
    }
    
  }

  end_time_total = RTIMER_NOW();
  
  flock_loc_diagnostics.runtime_us = (uint32_t) RTIMERTICKS_TO_US_64(RTIMER_CLOCK_DIFF(end_time_total, start_time_total));
  flock_loc_diagnostics.retries = retry;
  
  if(!confident) {
    LOG_DBG("Localization failed after %d retries, using best attempt with error mm: %d\n", FLOCK_LOC_MAX_RETRIES, flock_loc_diagnostics_best.average_error_mm);
    // Use the best positions found if we have any valid result
    memcpy(&flock_loc_diagnostics, &flock_loc_diagnostics_best, sizeof(flock_loc_diagnostics));
    memcpy(positions, positions_best, sizeof(positions));
    // Restore runtime and retries since they were overwritten by memcpy
    flock_loc_diagnostics.runtime_us = (uint32_t) RTIMERTICKS_TO_US_64(RTIMER_CLOCK_DIFF(end_time_total, start_time_total));
    flock_loc_diagnostics.retries = retry;

    // check if the best attempt is reasonable at all
    if(flock_loc_diagnostics_best.average_error_mm == 0) {
      LOG_ERR("Best attempt is not reasonable, average error mm: %d\n", flock_loc_diagnostics_best.average_error_mm);
      return false; // Localization failed
    }
  }

  // Update position table with estimated positions as ESTIMATE/ESTIMATE_BAD type
  for(size_t i=0; i<n; i++) {
    flock_pos_table_add(pos_table, positions[i].id, positions[i].x * 1000.0, positions[i].y * 1000.0, ESTIMATE , osf.epoch, flock_loc_diagnostics.average_error_mm);
  }

//  flock_loc_print_diagnostics(&flock_loc_diagnostics);
//  retry = 0;
  return confident;
}

void flock_loc_set_iterations(uint32_t iterations) {
  if(iterations > 0) {
    flock_loc_iterations = iterations;
    LOG_DBG("Set localization iterations to %lu\n", flock_loc_iterations);
  }else{
    LOG_WARN("Invalid number of iterations: %lu\n", iterations);
  }
}

void flock_loc_set_learning_rate(float lr) {
  if(lr > 0.0f) {
    flock_loc_learning_rate = lr;
    LOG_INFO("Set localization learning rate to %f\n", flock_loc_learning_rate);
  }else{
    LOG_WARN("Invalid learning rate: %f\n", lr);
  }
}

const flock_loc_diagnostics_t* flock_loc_get_diagnostics() {
  return &flock_loc_diagnostics;
}

void flock_loc_print_diagnostics(const flock_loc_diagnostics_t* diagnostics) {
  printf("{\"num_iterations\": %u, \"learning_rate\": %lu, \"average_error_mm\": %u, \"retries\": %u, \"runtime_us\": %lu}\n",
         diagnostics->num_iterations,
         (uint32_t)((diagnostics->learning_rate) *100),
         diagnostics->average_error_mm,
         diagnostics->retries,
         diagnostics->runtime_us);
}

/*---------------------------------------------------------------------------*/
/* Periodic position calculation process */
/*---------------------------------------------------------------------------*/
void flock_loc_print_result(rtimer_clock_t rtime_start, 
                             rtimer_clock_t rtime_end, 
                             bool success)
{
  static const flock_loc_diagnostics_t* diag = NULL;

  diag = flock_loc_get_diagnostics();
  
  printf("{\"nodes\": ");
  flock_pos_table_print_json(&flock_pos_table);

  printf(", ");
  printf("\"max_iterations\":%lu,", flock_loc_iterations);
  printf("\"learning_rate\":%lu,", (uint32_t) (flock_loc_learning_rate * 100));
  printf("\"calc_time_us\":%lu,", (uint32_t) RTIMERTICKS_TO_US_64(RTIMER_CLOCK_DIFF(rtime_end, rtime_start)));
  printf("\"diag\":{");
    printf("\"success\":%u,", success ? 1 : 0);
    printf("\"num_iterations\":%u,", diag->num_iterations);
    printf("\"average_error_mm\":%u,", diag->average_error_mm);
    printf("\"retries\":%u,", diag->retries);
    printf("\"runtime_us\":%lu,", diag->runtime_us);
    printf("\"runtime_iteration_us\":%lu", diag->runtime_iteration_us);
    printf("}");
  printf("}\n");
}


static struct pt calc_pos_pt;
PROCESS_THREAD(flock_loc_process, ev, data)
{
  PROCESS_BEGIN();

  static rtimer_clock_t rtime_start, rtime_end;
  static struct etimer et;
  static bool success;

  PT_INIT(&calc_pos_pt);
  LOG_INFO("Starting position calculation process\n");

  // wait a bit for the network to stabilize
  etimer_set(&et, CLOCK_SECOND * 3);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

  etimer_set(&et, FLOCK_LOC_UPDATE_RATE);
  while(true) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    etimer_reset(&et);

    rtime_start = RTIMER_NOW();
    success = flock_loc_estimate_positions(&flock_pos_table);
    rtime_end = RTIMER_NOW();

    flock_loc_print_result(rtime_start, rtime_end, success);
  }

  PROCESS_END();
}