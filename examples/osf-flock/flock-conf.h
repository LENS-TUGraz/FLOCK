#ifndef _FLOCK_CONF_H_
#define _FLOCK_CONF_H_

#include "project-conf.h"

/* Maximum number of entries in the distance table */
#ifndef FLOCK_CONF_DIST_TABLE_SIZE
#define FLOCK_DIST_TABLE_SIZE 150
#else
#define FLOCK_DIST_TABLE_SIZE FLOCK_CONF_DIST_TABLE_SIZE
#endif /* FLOCK_CONF_DIST_TABLE_SIZE */

/* Maximum number of entries in the position table */
#ifndef FLOCK_CONF_POS_TABLE_SIZE
#define FLOCK_POS_TABLE_SIZE 50
#else
#define FLOCK_POS_TABLE_SIZE FLOCK_CONF_POS_TABLE_SIZE
#endif /* FLOCK_CONF_POS_TABLE_SIZE */

/* Interval for sending report updates */
#ifndef FLOCK_CONF_REPORT_INTERVAL
#define FLOCK_REPORT_INTERVAL (CLOCK_SECOND * 1)
#else
#define FLOCK_REPORT_INTERVAL FLOCK_CONF_REPORT_INTERVAL
#endif /* FLOCK_CONf_REPORT_INTERVAL */

/* Guard time to be waited before the beginning of the ranging slots */
#ifndef FLOCK_CONF_SYNC_EVENT_GUARD_TIME
#define FLOCK_SYNC_EVENT_GUARD_TIME ((RTIMER_SECOND * 5) / 1000)
#else
#define FLOCK_SYNC_EVENT_GUARD_TIME FLOCK_CONF_SYNC_EVENT_GUARD_TIME
#endif /* FLOCK_CONF_SYNC_EVENT_GUARD_TIME */

/* Print range report on reception */
#ifndef FLOCK_CONF_PRINT_RANGE_REPORT
#define FLOCK_PRINT_RANGE_REPORT 0
#else
#define FLOCK_PRINT_RANGE_REPORT FLOCK_CONF_PRINT_RANGE_REPORT
#endif /* FLOCK_CONF_PRINT_RANGE_REPORT */

/* Print range result on reception */
#ifndef FLOCK_CONF_PRINT_RANGE_RESULT
#define FLOCK_PRINT_RANGE_RESULT 0
#else
#define FLOCK_PRINT_RANGE_RESULT FLOCK_CONF_PRINT_RANGE_RESULT
#endif /* FLOCK_CONF_PRINT_RANGE_RESULT */

/* Learning rate for the gradient descent localization */
#ifndef FLOCK_CONF_LOC_LEARNING_RATE
#define FLOCK_LOC_LEARNING_RATE   (0.020f)
#else
#define FLOCK_LOC_LEARNING_RATE   (FLOCK_CONF_LOC_LEARNING_RATE)
#endif /* FLOCK_CONF_LOC_LEARNING_RATE */

/* Number of iterations for the gradient descent localization */
#ifndef FLOCK_CONF_LOC_ITERATIONS
#define FLOCK_LOC_ITERATIONS      (100)
#else
#define FLOCK_LOC_ITERATIONS      (FLOCK_CONF_LOC_ITERATIONS)
#endif /* FLOCK_CONF_LOC_ITERATIONS */

#ifndef FLOCK_CONF_LOC_EPOCHS_TO_KEEP
#define FLOCK_LOC_EPOCHS_TO_KEEP (10)
#else
#define FLOCK_LOC_EPOCHS_TO_KEEP FLOCK_CONF_LOC_EPOCHS_TO_KEEP
#endif /* FLOCK_CONF_LOC_EPOCHS_TO_KEEP */

#ifndef FLOCK_CONF_LOC_RNG_EPOCHS_TO_KEEP
#define FLOCK_LOC_RNG_EPOCHS_TO_KEEP FLOCK_LOC_EPOCHS_TO_KEEP
#else
#define FLOCK_LOC_RNG_EPOCHS_TO_KEEP FLOCK_CONF_LOC_RNG_EPOCHS_TO_KEEP
#endif /* FLOCK_CONF_LOC_RNG_EPOCHS_TO_KEEP */

#ifndef FLOCK_CONF_LOC_POS_EPOCHS_TO_KEEP
#define FLOCK_LOC_POS_EPOCHS_TO_KEEP FLOCK_LOC_EPOCHS_TO_KEEP
#else
#define FLOCK_LOC_POS_EPOCHS_TO_KEEP FLOCK_CONF_LOC_POS_EPOCHS_TO_KEEP
#endif /* FLOCK_CONF_LOC_POS_EPOCHS_TO_KEEP */

/* Minimum distance of ranges used in the localization */
#ifndef FLOCK_CONF_LOC_MIN_DIST_MM
#define FLOCK_LOC_MIN_DIST_MM (0) 
#else
#define FLOCK_LOC_MIN_DIST_MM FLOCK_CONF_LOC_MIN_DIST_MM
#endif /* FLOCK_CONF_LOC_MIN_DIST_MM */

/* Maximum distance of ranges used in the localization */
#ifndef FLOCK_CONF_LOC_MAX_DIST_MM
#define FLOCK_LOC_MAX_DIST_MM (30000)
#else
#define FLOCK_LOC_MAX_DIST_MM FLOCK_CONF_LOC_MAX_DIST_MM
#endif /* FLOCK_CONF_LOC_MAX_DIST_MM */

/* Early break threshold in milli meter 
 * set to 0 to disable early break
 * 1 mm is a good value to enable early break without affecting accuracy
 */
#ifndef FLOCK_CONF_LOC_EARLY_BREAK_THRESHOLD_MM
#define FLOCK_LOC_EARLY_BREAK_THRESHOLD_MM (1)
#else
#define FLOCK_LOC_EARLY_BREAK_THRESHOLD_MM FLOCK_CONF_LOC_EARLY_BREAK_THRESHOLD_MM
#endif /* FLOCK_CONF_LOC_EARLY_BREAK_THRESHOLD_MM */

/* How often the break thershold needs to be reached consecutively until the early break is triggered */
#ifndef FLOCK_CONF_LOC_EARLY_BREAK_ITERS
#define FLOCK_LOC_EARLY_BREAK_ITERS (5)
#else
#define FLOCK_LOC_EARLY_BREAK_ITERS FLOCK_CONF_LOC_EARLY_BREAK_ITERS
#endif /* FLOCK_CONF_LOC_EARLY_BREAK_ITERS */

/* Expected average range error in milli meter */
#ifndef FLOCK_CONF_LOC_APRIORI_RANGE_ERROR_MM
#define FLOCK_LOC_APRIORI_RANGE_ERROR_MM (500)
#else
#define FLOCK_LOC_APRIORI_RANGE_ERROR_MM FLOCK_CONF_LOC_APRIORI_RANGE_ERROR_MM
#endif /* FLOCK_CONF_LOC_APRIORI_RANGE_ERROR_MM */

/* Number of retries for the localization */
#ifndef FLOCK_CONF_LOC_MAX_RETRIES
#define FLOCK_LOC_MAX_RETRIES (10)
#else
#define FLOCK_LOC_MAX_RETRIES FLOCK_CONF_LOC_MAX_RETRIES
#endif /* FLOCK_CONF_LOC_MAX_RETRIES */

/* Size of the initialization area in meters */
#ifndef FLOCK_CONF_LOC_INIT_AREA_M
#define FLOCK_LOC_INIT_AREA_M (30.0f)
#else
#define FLOCK_LOC_INIT_AREA_M FLOCK_CONF_LOC_INIT_AREA_M
#endif /* FLOCK_CONF_LOC_INIT_AREA_M */

/* Update rate for periodic localization */
#ifndef FLOCK_CONF_LOC_UPDATE_RATE
#define FLOCK_LOC_UPDATE_RATE (CLOCK_SECOND * 1)
#else
#define FLOCK_LOC_UPDATE_RATE FLOCK_CONF_LOC_UPDATE_RATE
#endif /* FLOCK_CONF_LOC_UPDATE_RATE */

#ifndef DW1000_CONF_ST_RNG_NBR_PRUNE_THRESHOLD
#define DW1000_ST_RNG_NBR_PRUNE_THRESHOLD (RTIMER_SECOND * 10)
#else
#define DW1000_ST_RNG_NBR_PRUNE_THRESHOLD DW1000_CONF_ST_RNG_NBR_PRUNE_THRESHOLD
#endif /* DW1000_CONF_ST_RNG_NBR_PRUNE_THRESHOLD */

#endif /* _FLOCK_CONF_H_ */