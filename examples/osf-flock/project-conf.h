#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

/*---------------------------------------------------------------------------*/
/* Basic FLOCK Configuration */
/*---------------------------------------------------------------------------*/
#define FLOCK_MAX_NODES                     25

#define FLOCK_CONF_REPORT_INTERVAL          (CLOCK_SECOND * 1)
#define FLOCK_CONF_SYNC_EVENT_GUARD_TIME    ((RTIMER_SECOND * 100) / 1000) /* Guard time after the SYNC event to the start of the ranging slot. */
#define FLOCK_CONF_DIST_TABLE_SIZE          (FLOCK_MAX_NODES * (FLOCK_MAX_NODES-1)) /* Maximum number of entries in the distance table */
#define FLOCK_CONF_POS_TABLE_SIZE           (FLOCK_MAX_NODES)   /* Maximum number of entries in the position table */
#define FLOCK_CONF_LOC_LEARNING_RATE        0.020f /* Learning rate for the gradient descent localization */
#define FLOCK_CONF_LOC_ITERATIONS           500  /* Number of iterations for the gradient descent */
#define FLOCK_CONF_LOC_EPOCHS_TO_KEEP       0    /* Number of epochs to keep in the position table */
//#define FLOCK_CONF_LOC_RNG_EPOCHS_TO_KEEP   5
//#define FLOCK_CONF_LOC_POS_EPOCHS_TO_KEEP   5
#define FLOCK_CONF_LOC_MIN_DIST_MM          0   /* Minimum distance of ranges used in the localization */
#define FLOCK_CONF_LOC_MAX_DIST_MM          100000 /* Maximum distance of ranges used in the localization */
#define FLOCK_CONF_LOC_MAX_RETRIES          16   /* Number of retries for the localization */
#define FLOCK_CONF_LOC_APRIORI_RANGE_ERROR_MM (250) /* A priori range error in millimeters */
#define FLOCK_CONF_LOC_UPDATE_RATE          0 /* Update rate for periodic localization */  
#define FLOCK_CONF_LOC_INIT_AREA_M         80.0f /* Size of the initialization area in meters */  

#define FLOCK_CONF_PRINT_RANGE_RESULT       0  /* Print range result on reception */
#define FLOCK_CONF_PRINT_RANGE_REPORT       0  /* Print range report on reception */
/*---------------------------------------------------------------------------*/
/* Basic OSF Configuration */
/*---------------------------------------------------------------------------*/
/* SF timesync / initiator (ONLY WORKS WITHOUT TESTBED) */
#define OSF_CONF_PERIOD_MS (900)
#define OSF_CONF_HOPPING (0)
#define OSF_CONF_LOGGING (0)
#define OSF_DEBUG_GPIO (0)
#define OSF_DEBUG_LEDS (1)
#define OSF_CONF_NTX (3)
#define OSF_CONF_TXPOWER ZerodBm
#define OSF_CONF_PROTO OSF_PROTO_STT
#define OSF_CONF_PHY PHY_BLE_2M
#define OSF_CONF_ROUND_S_STATLEN (1)
#define OSF_CONF_ROUND_T_STATLEN (0)
/*---------------------------------------------------------------------------*/
/* UWB Config */
/*---------------------------------------------------------------------------*/
#define DW1000_CONF_ST_RNG_BIAS_CORRECTION 1                 /* Enable bias correction in the staggered ranging protocol */
#define DW1000_CONF_ST_RNG_MAX_NBR        (FLOCK_MAX_NODES)  /* Maximum number of neighbors */

#define DW1000_CONF_ST_RNG_POST_FINAL_GUARD_US  (1500) /* Guard time after the transmission of the FINAL message to the end of the ranging round. */
#define DW1000_CONF_ST_RNG_MAX_RNG_PARTNERS      (6)   /* Number of rangings per RNG slot */

#define PREAMBLE_LENGTH 64 /* Preamble length in symbols */
#if PREAMBLE_LENGTH == 64
  #define DW1000_CONF_CONFIG { \
    5,               /* Channel */ \
    DWT_PRF_64M,     /* PRF */ \
    DWT_PLEN_64,      /* TX preamble length */ \
    DWT_PAC8,         /* PAC size */ \
    9,               /* TX preamble code */ \
    9,               /* RX preamble code */ \
    0,               /* Non-standard SFD */ \
    DWT_BR_6M8,      /* Data rate */ \
    DWT_PHRMODE_STD, /* PHR mode */ \
    64 + 8 + 8};     /* SFD-timeout */

  #define DW1000_CONF_ST_RNG_PRE_REQUEST_GUARD_US (600) /* Guard time before the transmission of a REQUEST message */
  #define DW1000_CONF_ST_RNG_RESPONSE_DELAY_US (1200)
  #define DW1000_CONF_ST_RNG_RESPONSE_SLOT_DELAY_US (600)
  #define DW1000_CONF_ST_RNG_FINAL_DELAY_US (900)
#elif PREAMBLE_LENGTH == 1024
  #define DW1000_CONF_CONFIG { \
    5,               /* Channel */ \
    DWT_PRF_64M,     /* PRF */ \
    DWT_PLEN_1024,   /* TX preamble length */ \
    DWT_PAC64,       /* PAC size */ \
    9,               /* TX preamble code */ \
    9,               /* RX preamble code */ \
    0,               /* Non-standard SFD */ \
    DWT_BR_6M8,      /* Data rate */ \
    DWT_PHRMODE_STD, /* PHR mode */ \
    1024 + 64 + 8};              /* SFD-timeout */

  #define DW1000_CONF_ST_RNG_PRE_REQUEST_GUARD_US (1700) /* Guard time before the transmission of a REQUEST message */
  #define DW1000_CONF_ST_RNG_RESPONSE_DELAY_US (2300)
  #define DW1000_CONF_ST_RNG_RESPONSE_SLOT_DELAY_US (1700)
  #define DW1000_CONF_ST_RNG_FINAL_DELAY_US (2000)

  #else
  #error "Unsupported preamble length"
#endif
/*---------------------------------------------------------------------------*/
/* DW1000 Debug GPIO pins */
/*---------------------------------------------------------------------------*/
#include "nrf.h"
/*
#define DW1000_TX_PIN NRF_GPIO_PIN_MAP(1, 2)
#define DW1000_RX_PIN NRF_GPIO_PIN_MAP(1, 1)

#define DW1000_DEBUG_TX_INIT()  nrf_gpio_cfg_output(DW1000_TX_PIN); \
                                nrf_gpio_pin_clear(DW1000_TX_PIN);
#define DW1000_DEBUG_TX_START() nrf_gpio_pin_set(DW1000_TX_PIN);
#define DW1000_DEBUG_TX_END()   nrf_gpio_pin_clear(DW1000_TX_PIN);

#define DW1000_DEBUG_RX_INIT()  nrf_gpio_cfg_output(DW1000_RX_PIN); \
                                nrf_gpio_pin_clear(DW1000_RX_PIN);
#define DW1000_DEBUG_RX_START() nrf_gpio_pin_set(DW1000_RX_PIN);
#define DW1000_DEBUG_RX_END()   nrf_gpio_pin_clear(DW1000_RX_PIN);  
*/

#define OSF_FLOCK_SYNC_HOOK() flock_osf_sync_callback()


/*---------------------------------------------------------------------------*/
/* Debug */
/*---------------------------------------------------------------------------*/
#define LOG_CONF_LEVEL_TCPIP                LOG_LEVEL_NONE
#define LOG_CONF_LEVEL_IPV6                 LOG_LEVEL_NONE
#define LOG_CONF_LEVEL_6LOWPAN              LOG_LEVEL_NONE
#define LOG_CONF_LEVEL_MAC                  LOG_LEVEL_NONE
#define LOG_CONF_LEVEL_FRAMER               LOG_LEVEL_NONE

#endif /* PROJECT_CONF_H_ */
