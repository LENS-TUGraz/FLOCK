#ifndef _FLOCK_DIST_TABLE_H_
#define _FLOCK_DIST_TABLE_H_

#include "contiki.h"
#include "project-conf.h"
#include "flock-conf.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>


/**
 * @file flock-dist-table.h
 * @brief Header file for the flock distance table.
 * 
 * This file defines the structure and functions for managing a distance table
 * that stores distances between nodes in a flock.
 */

typedef struct flock_dist_table_element {
  uint16_t from_id;        // ID of the node from which the distance is measured
  uint16_t to_id;          // ID of the node to which the distance is measured
  uint16_t dist_mm;        // Distance in millimeters
  volatile bool is_valid;  // Flag indicating if the entry is valid
  uint16_t osf_epoch;      // Epoch when the distance was measured
} flock_dist_table_element_t;

/**
 * @brief Initialize the flock distance table.
 * 
 * This function initializes the flock distance table by setting all elements to their default values.
 */
void flock_dist_table_init();

/**
 * @brief Add a distance entry to the flock distance table.
 * 
 * @param from_id The ID of the node from which the distance is measured.
 * @param to_id The ID of the node to which the distance is measured.
 * @param dist_mm The distance in millimeters.
 * @param osf_epoch The OSF epoch when the distance was measured.
 */
void flock_dist_table_add(uint16_t from_id, uint16_t to_id, uint16_t dist_mm, uint16_t osf_epoch);

/**
 * @brief Get a distance entry from the flock distance table.
 * 
 * @param from_id The ID of the node from which the distance is measured.
 * @param to_id The ID of the node to which the distance is measured.
 * @return A pointer to the flock_dist_table_element_t if found, otherwise NULL.
 */
flock_dist_table_element_t* flock_dist_table_get(uint16_t from_id, uint16_t to_id);

/**
 * @brief Get the number of valid entries in the flock distance table.
 * 
 * @return The number of valid entries in the table.
 */
const size_t flock_dist_table_length();

/**
 * @brief Get the first valid element in the flock distance table.
 * 
 * @return A pointer to the first valid flock_dist_table_element_t, or NULL if no valid elements exist.
 */
const flock_dist_table_element_t*flock_dist_table_get_head();

/**
 * @brief Get the next valid element in the flock distance table after the current one.
 * 
 * @param current A pointer to the current flock_dist_table_element_t.
 * @return A pointer to the next valid flock_dist_table_element_t, or NULL if no more valid elements exist.
 */
const flock_dist_table_element_t* flock_dist_table_get_next(const flock_dist_table_element_t* current);

/**
 * @brief Remove a distance entry from the flock distance table.
 * 
 * @param from_id The ID of the node from which the distance is measured.
 * @param to_id The ID of the node to which the distance is measured.
 */
void flock_dist_table_remove(uint16_t from_id, uint16_t to_id);

/**
 * @brief Prune the flock distance table by removing entries older than a specified number of epochs.
 * 
 * @param epochs The number of epochs to keep. Entries older than this will be removed.
 */
PT_THREAD(flock_dist_table_prune(struct pt* pt, uint16_t epochs));

/**
 * @brief Print the flock distance table in JSON format.
 * 
 * This function prints the current state of the flock distance table in JSON format to standard output.
 */
void flock_dist_table_print_json();


#endif /* _FLOCK_DIST_TABLE_H_ */