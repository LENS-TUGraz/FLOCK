#include "flock-pos-table.h"

#include "sys/log.h"
#define LOG_MODULE "FLOCK POS TABLE"
#define LOG_LEVEL LOG_LEVEL_INFO

void flock_pos_table_init(flock_pos_table_t *table) {
    table->length = 0;
    table->max_length = FLOCK_POS_TABLE_SIZE;
    for(size_t i = 0; i < FLOCK_POS_TABLE_SIZE; ++i) {
        table->entries[i].valid = false;
    }
}

int flock_pos_table_add(flock_pos_table_t *table, uint16_t id, float x, float y, flock_pos_type_t type, uint16_t osf_epoch, uint16_t error_mm) {
  for(size_t i = 0; i < FLOCK_POS_TABLE_SIZE; i++){
    // check if the entry is already in the table
    if(table->entries[i].id == id && table->entries[i].type == type && table->entries[i].valid) {
      table->entries[i].x = x;
      table->entries[i].y = y;
      table->entries[i].type = type;
      table->entries[i].valid = true;
      table->entries[i].osf_epoch = osf_epoch; // Update the OSF epoch when the position was last updated
      table->entries[i].error_mm = error_mm; // Update the estimated error

      LOG_DBG("update position for id %u, x: %d, y: %d, type: %d, epoch: %u, error: %u\n", id, (int) x, (int) y, type, osf_epoch, error_mm);
      return 0;
    }
  }

  // check if the table is full
  if(table->length >= FLOCK_POS_TABLE_SIZE) {
    LOG_WARN("table is full, cannot add new entry\n");
    return 1;
  }

  // if not found, add a new entry
  for(size_t i = 0; i < FLOCK_POS_TABLE_SIZE; i++) {
    // find first free entry
    if(!table->entries[i].valid) {
      table->entries[i].id = id;
      table->entries[i].x = x;
      table->entries[i].y = y;
      table->entries[i].type = type;
      table->entries[i].valid = true;
      table->entries[i].osf_epoch = osf_epoch;
      table->entries[i].error_mm = error_mm;
      table->length++;
      LOG_DBG("add new position for id %u, x: %d, y: %d, type: %d, error: %u\n", id, (int) x, (int) y, type, error_mm);
      return 0;
    }
  }
  return 0;
}

const flock_pos_entry_t* flock_pos_table_get(const flock_pos_table_t *table, uint16_t id) {
  for(size_t i = 0; i < table->length; ++i) {
    if((table->entries[i].id == id) && (table->entries[i].valid)) {
      return &(table->entries[i]);
    }
  }
  return NULL;
}

const flock_pos_entry_t* flock_pos_table_get_head(const flock_pos_table_t *table)
{
  if(table->length > 0) {
    for(size_t i = 0; i < table->max_length; ++i) {
      if(table->entries[i].valid) {
        return &(table->entries[i]);
      }
    }
  }
  return NULL;
}


const flock_pos_entry_t* flock_pos_table_get_next(const flock_pos_table_t *table, const flock_pos_entry_t *current)
{
  if(current == NULL) {
    return flock_pos_table_get_head(table);
  }

  for(size_t i = 0; i < table->length; ++i) {
    if(&table->entries[i] == current) {
      for(size_t j = i + 1; j < table->max_length; ++j) {
        if(table->entries[j].valid) {
          return &(table->entries[j]);
        }
      }
      break;
    }
  }
  return NULL;
}

void flock_pos_table_remove(flock_pos_table_t *table, uint16_t id) {
  for(size_t i = 0; i < table->length; ++i) {
    if(table->entries[i].id == id) {
      table->entries[i].valid = false;
      table->length--;
      return;
    }
  }
}

static void
print_pos_table_element_type(const flock_pos_entry_t* elem)
{
  if(elem == NULL) {
    return;
  }

//  NONE = 0,
//  ESTIMATE, // Estimated positions will be taken as initial guess for localization engine but dont contribute to the gradient
//  UNKNOWN,  // Unknown positions will be ignored by the localization engine
//  KNOWN,    // Known positions will be taken as initial guess for localization engine and contribute to the gradient
//  FIXED

  switch(elem->type) {
    case NONE:
      printf("NONE");
      return;
    case ESTIMATE:
      printf("ESTIMATE");
      return;
    case UNKNOWN:
      printf("UNKNOWN");
      return;
    case KNOWN:
      printf("KNOWN");
      return;
    case FIXED:
      printf("FIXED");
      return;
    default:
      printf("NONE");
      return;
  }
}

static
void print_pos_table_element(const flock_pos_entry_t* elem)
{
  if(elem == NULL) {
    return;
  }
  printf("{\"id\":%u,\"x_mm\":%d,\"y_mm\":%d,\"epoch\":%u,\"error_mm\":%u,\"type\":\"",
         elem->id, (int) elem->x, (int) elem->y,
         elem->osf_epoch, elem->error_mm);
  print_pos_table_element_type(elem);
  printf("\"}");
}

void flock_pos_table_print_json(const flock_pos_table_t *table) {
  const flock_pos_entry_t* elem = flock_pos_table_get_head(table);

  printf("[");
  if(elem == NULL) {
    printf("]");
    return;
  }

  while(elem != NULL) {
    print_pos_table_element(elem);

    elem = flock_pos_table_get_next(table, elem);
    if(elem != NULL) {
      printf(",");
    }
  }
  printf("]");
}

PT_THREAD(flock_pos_table_prune(struct pt* pt, flock_pos_table_t *table, uint16_t epochs)) 
{
  PT_BEGIN(pt);
  static uint16_t current_epoch, min_epoch;
  static flock_pos_entry_t* el;

  if(epochs == 0) {
    PT_EXIT(pt); // Nothing to prune
  }

  current_epoch = osf.epoch;
  min_epoch = current_epoch - epochs;

  for(size_t i = 0; i < table->max_length; ++i) {
    el = &(table->entries[i]);

    if(el->valid && (el->osf_epoch < min_epoch)) {
      LOG_DBG("Pruning position for id %u, epoch %u\n", table->entries[i].id, table->entries[i].osf_epoch);
      table->entries[i].valid = false; // Mark as invalid
      table->entries[i].id = 0; // Clear ID
      table->entries[i].x = 0.0f; // Clear X coordinate
      table->entries[i].y = 0.0f; // Clear Y coordinate
      table->entries[i].osf_epoch = 0; // Clear OSF epoch
      table->entries[i].type = NONE; // Clear type
      table->length--;
    }
    if(el->valid && (el->osf_epoch > osf.epoch)) {
      // This should not happen, but in case of epoch wrap-around or errors
      table->entries[i].valid = false;     
      table->length--;
      LOG_DBG("Position for id %u has future epoch %u, current_epoch %u\n", table->entries[i].id, table->entries[i].osf_epoch, current_epoch);
    }
  }
  PT_END(pt);
}