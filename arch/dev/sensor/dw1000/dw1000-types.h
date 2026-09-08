#ifndef _DW1000_TYPES_H_
#define _DW1000_TYPES_H_

/* ---------------------------------------------------------------- */
typedef enum {
    DW1000_OK = 0,
    DW1000_ERR = -1
} dw1000_ret_t;

typedef enum {
    DW1000_MODE_IDLE = 0,
    DW1000_MODE_RX,
    DW1000_MODE_TX
} dw1000_mode_t;
/* ---------------------------------------------------------------- */

#endif/* _DW1000_TYPES_H_ */