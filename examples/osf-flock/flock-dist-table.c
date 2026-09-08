#include "flock-dist-table.h"

#include <stdbool.h>
#include "os/net/mac/osf/osf.h"


/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "FLOCK DIST TABLE"
#define LOG_LEVEL LOG_LEVEL_NONE


/**
 * @brief Array holding the elements of the flock distance table.
 */
static flock_dist_table_element_t flock_dist_table_elements[FLOCK_DIST_TABLE_SIZE]; 

/**
 * @brief Number of valid elements currently in the table.
 */
static volatile size_t n_elements;

/**
 * @brief Maximum number of elements the table can hold.
 */
static const size_t max_elements = FLOCK_DIST_TABLE_SIZE;


/**
 * @brief Initialize the flock distance table, clearing all entries.
 */
void
flock_dist_table_init()
{
  for(size_t i = 0; i < max_elements; i++){
    flock_dist_table_elements[i].from_id = 0;
    flock_dist_table_elements[i].to_id = 0;
    flock_dist_table_elements[i].dist_mm = 0;
    flock_dist_table_elements[i].is_valid = false;
    flock_dist_table_elements[i].osf_epoch = 0;
  }
  n_elements = 0;
}


/**
 * @brief Add or update a distance entry in the flock distance table.
 *
 * @param from_id    The source node ID.
 * @param to_id      The destination node ID.
 * @param dist_mm    The distance in millimeters.
 * @param osf_epoch  The epoch associated with this entry.
 */
void
flock_dist_table_add(uint16_t from_id, uint16_t to_id, uint16_t dist_mm, uint16_t osf_epoch)
{
  if(to_id == 0 || from_id == 0){
    LOG_WARN("Invalid IDs: from_id: %u, to_id: %u\n", from_id, to_id);
    return;
  }
  /* Check if distance is already stored in table */
  for(size_t i = 0; i < max_elements; i++){
    if((flock_dist_table_elements[i].from_id == from_id) && (flock_dist_table_elements[i].to_id == to_id) && (flock_dist_table_elements[i].is_valid)){
      LOG_DBG("Updating distance from %u to %u: %u mm\n", from_id, to_id, dist_mm);
      flock_dist_table_elements[i].dist_mm = dist_mm;
      flock_dist_table_elements[i].is_valid = true;
      flock_dist_table_elements[i].osf_epoch = osf_epoch;

      LOG_DBG("number of elements: %u/%u\n", n_elements, max_elements);
      return;
    }
  }

  /* check for the next available entry */
  for(size_t i = 0; i < max_elements; i++){
    if(!flock_dist_table_elements[i].is_valid){
      flock_dist_table_elements[i].from_id = from_id;
      flock_dist_table_elements[i].to_id = to_id;
      flock_dist_table_elements[i].dist_mm = dist_mm;
      flock_dist_table_elements[i].is_valid = true;
      flock_dist_table_elements[i].osf_epoch = osf_epoch;
      n_elements++;
      LOG_DBG("Added distance from %u to %u: %u mm\n", from_id, to_id, dist_mm);
      LOG_DBG("number of elements: %u/%u\n", n_elements, max_elements);
      return;
    }
  }

  LOG_WARN("Flock distance table full, cannot add distance from %u to %u\n", from_id, to_id);
}


/**
 * @brief Retrieve a pointer to a distance entry for a given node pair.
 *
 * @param from_id    The source node ID.
 * @param to_id      The destination node ID.
 * @return Pointer to the table element, or NULL if not found.
 */
flock_dist_table_element_t*
flock_dist_table_get(uint16_t from_id, uint16_t to_id)
{
  for(size_t i = 0; i < max_elements; i++){
    if(flock_dist_table_elements[i].from_id == from_id && flock_dist_table_elements[i].to_id == to_id && flock_dist_table_elements[i].is_valid){
      return &flock_dist_table_elements[i];
    }
  }
  return NULL;
}


/**
 * @brief Get the number of valid elements in the flock distance table.
 *
 * @return Number of valid elements.
 */
const size_t
flock_dist_table_length()
{
  return n_elements;
}


/**
 * @brief Get a pointer to the first valid element in the table.
 *
 * @return Pointer to the first valid element, or NULL if table is empty.
 */
const flock_dist_table_element_t*
flock_dist_table_get_head()
{
  if(n_elements == 0) return NULL;
  // Return the first valid element in the table
  for(size_t i = 0; i < max_elements; i++){
    if(flock_dist_table_elements[i].is_valid){
      return &flock_dist_table_elements[i];
    }
  }
  return NULL;
}


/**
 * @brief Get the next valid element in the table after the given element.
 *
 * @param current Pointer to the current element.
 * @return Pointer to the next valid element, or NULL if none.
 */
const flock_dist_table_element_t*
flock_dist_table_get_next(const flock_dist_table_element_t* current)
{
  size_t current_index;
  if(current == NULL) return NULL;

  current_index = (current - flock_dist_table_elements);

  for(size_t i = current_index + 1; i < max_elements; i++){
    if(flock_dist_table_elements[i].is_valid){
      return &flock_dist_table_elements[i];
    }
  }

  return NULL;
}

/**
 * @brief Remove an entry from the flock distance table.
 *
 * @param from_id    The source node ID.
 * @param to_id      The destination node ID.
 */
void
flock_dist_table_remove(uint16_t from_id, uint16_t to_id)
{
  for(size_t i = 0; i < max_elements; i++){
    if(flock_dist_table_elements[i].from_id == from_id && flock_dist_table_elements[i].to_id == to_id){
      flock_dist_table_elements[i].is_valid = false;
      n_elements--;
      return;
    }
  }
}

/**
 * @brief Prune (remove) entries older than a given number of epochs.
 *
 * @param epochs  The number of epochs to keep; older entries are removed.
 */
PT_THREAD(flock_dist_table_prune(struct pt* pt, uint16_t epochs))
{
  PT_BEGIN(pt);
  static size_t i;
  static uint16_t current_epoch, epoch_threshold;
  static flock_dist_table_element_t* el;

  if(n_elements == 0) PT_EXIT(pt);
  if(epochs == 0) PT_EXIT(pt); // Nothing to prune
  if(epochs > osf.epoch) PT_EXIT(pt); // Not enough epochs to prune
 
  current_epoch = osf.epoch;
  epoch_threshold = current_epoch - epochs;

  for(i = 0; i < max_elements; i++){
    el = &(flock_dist_table_elements[i]);
    if(el->is_valid && (el->osf_epoch < epoch_threshold)) {
      LOG_DBG("Pruning element from %u to %u, dist %u mm, epoch %u, current_epoch %u\n", flock_dist_table_elements[i].from_id, flock_dist_table_elements[i].to_id, flock_dist_table_elements[i].dist_mm, flock_dist_table_elements[i].osf_epoch, current_epoch);
//      flock_dist_table_elements[i].from_id = 0;
//      flock_dist_table_elements[i].to_id = 0;
//      flock_dist_table_elements[i].dist_mm = 0;
      flock_dist_table_elements[i].is_valid = false;
      n_elements -= 1;
    }
    if(el->is_valid && (el->osf_epoch > osf.epoch)) {
      // This should not happen, but in case of epoch wrap-around or errors
      flock_dist_table_elements[i].is_valid = false;     
      LOG_WARN("Element from %u to %u has future epoch %u, current_epoch %u\n", flock_dist_table_elements[i].from_id, flock_dist_table_elements[i].to_id, flock_dist_table_elements[i].osf_epoch, current_epoch);
    }
  }
  PT_END(pt);
}


/**
 * @brief Print a single table element as a JSON object.
 *
 * @param element Pointer to the element to print.
 */
static void
flock_print_element_json(flock_dist_table_element_t* element){
  if(element == NULL) return;
  printf("{\"from_id\": %u, \"to_id\": %u, \"dist_mm\": %u}", element->from_id, element->to_id, element->dist_mm);
}


/**
 * @brief Print the entire flock distance table as a JSON array.
 */
void 
flock_dist_table_print_json()
{
  if(n_elements == 0){
    printf("[]\n");
    return;
  }
  printf("[");
  for(size_t i = 0; i < max_elements; i++){
    if(flock_dist_table_elements[i].is_valid){
      flock_print_element_json(&flock_dist_table_elements[i]);
      if(i < n_elements - 1) printf(",");
    }
  }
  printf("]\n");
}