#ifndef _STAGGERED_RANGING_H_
#define _STAGGERED_RANGING_H_
#include "dw1000.h"
#include "dw1000-types.h"
#include "dw1000-msg.h"
#include "deca_device_api.h"

/* Guard time before the transmission of a REQUEST message */
#ifdef DW1000_CONF_ST_RNG_PRE_REQUEST_GUARD_US
#define DW1000_ST_RNG_PRE_REQUEST_GUARD_US DW1000_CONF_ST_RNG_PRE_REQUEST_GUARD_US
#else
#define DW1000_ST_RNG_PRE_REQUEST_GUARD_US (1000)
#endif

/* Maximum number of ranging partners per ranging round */
#ifdef DW1000_CONF_ST_RNG_MAX_RNG_PARTNERS
#define DW1000_ST_RNG_MAX_RNG_PARTNERS DW1000_CONF_ST_RNG_MAX_RNG_PARTNERS
#else
#define DW1000_ST_RNG_MAX_RNG_PARTNERS (4)
#endif /* DW1000_CONF_ST_RNG_MAX_RNG_PARTNERS */

/* Delay between the reception of the REQUEST message and 
 * the transmission of the first RESPONSE message */
#ifdef DW1000_CONF_ST_RNG_RESPONSE_DELAY_US
#define DW1000_ST_RNG_RESPONSE_DELAY_US DW1000_CONF_ST_RNG_RESPONSE_DELAY_US
#else
#define DW1000_ST_RNG_RESPONSE_DELAY_US (1000)
#endif /* DW1000_CONF_ST_RNG_RESPONSE_DELAY_US */


/* Delay between the RESPONSE messages */
#ifdef DW1000_CONF_ST_RNG_RESPONSE_SLOT_DELAY_US
#define DW1000_ST_RNG_RESPONSE_SLOT_DELAY_US DW1000_CONF_ST_RNG_RESPONSE_SLOT_DELAY_US
#else
#define DW1000_ST_RNG_RESPONSE_SLOT_DELAY_US (600)
#endif /* DW1000_CONF_ST_RNG_RESPONSE_SLOT_DELAY_US */


/* Delay between the reception of the last RESPONSE message and
 * the transmission of the FINAL message */
#ifdef DW1000_CONF_ST_RNG_FINAL_DELAY_US
#define DW1000_ST_RNG_FINAL_DELAY_US DW1000_CONF_ST_RNG_FINAL_DELAY_US
#else
#define DW1000_ST_RNG_FINAL_DELAY_US (900)
#endif /* DW1000_CONF_ST_RNG_FINAL_DELAY_US */

/* Guard time after the transmission of the FINAL message to 
 * to the end of the ranging round.
 */
#ifdef DW1000_CONF_ST_RNG_POST_FINAL_GUARD_US
#define DW1000_ST_RNG_POST_FINAL_GUARD_US DW1000_CONF_ST_RNG_POST_FINAL_GUARD_US
#else
#define DW1000_ST_RNG_POST_FINAL_GUARD_US (1000)
#endif /* DW1000_CONF_ST_RNG_POST_FINAL_GUARD_US */

#define DW1000_ST_RNG_SLOT_DURATION_US (DW1000_ST_RNG_PRE_REQUEST_GUARD_US + \
                                        DW1000_ST_RNG_RESPONSE_DELAY_US + \
                                        (DW1000_ST_RNG_RESPONSE_SLOT_DELAY_US * DW1000_ST_RNG_MAX_RNG_PARTNERS)+ \
                                        DW1000_ST_RNG_FINAL_DELAY_US + \
                                        DW1000_ST_RNG_POST_FINAL_GUARD_US)
#define STAGGERED_RANGING_MAX_IDS (15)

typedef struct rng_result {
  uint16_t from;
  uint16_t to;
  float dist_dstwr_mm;
  float dist_sstwr_mm;
  float dist_sstwr_corr_mm;
  float cfo_ppm;
  uint8_t slot_number;
} rng_result_t;

extern struct pt staggered_ranging_pt;

PT_THREAD(dw1000_staggered_range_process(struct pt* pt));

//PROCESS_NAME(dw1000_staggered_range_process);
dw1000_ret_t dw1000_staggered_range_with(uint16_t* ids, size_t n_ids);
dw1000_ret_t dw1000_staggered_range_set_done_cb(struct process *p);

#endif /* _STAGGERED_RANGING_H_ */