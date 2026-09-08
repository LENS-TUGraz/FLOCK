#ifndef _DW1000_H_
#define _DW1000_H_

#include <stdint.h>
#include <stddef.h>
#include "dw1000-types.h"
#include "dw1000-msg.h"
#include "deca_device_api.h"

/* ---------------------------------------------------------------- */
#define ACC_READ_STEP 128
#define CIR_BEFORE_FP 16
#define CIR_AFTER_FP  52
#define CIR_BUFFER_SIZE (CIR_BEFORE_FP + CIR_AFTER_FP + 1)

#define DW1000_CRC_LEN 2
#define DW1000_UUS_TO_DWT_TIME 65536
#define DW1000_RADIO_TIME_TO_US(T)   ((T) * (DWT_TIME_UNITS * 1e6))
#define DW1000_US_TO_RADIO_TIME(US)  ((US) * DW1000_UUS_TO_DWT_TIME)

/* ---------------------------------------------------------------- */
extern struct rtimer dw1000_timer;
dw1000_ret_t dw1000_init();
dw1000_ret_t dw1000_reset();
dw1000_ret_t dw1000_on();
dw1000_ret_t dw1000_on_dly(uint32_t delay_us);
dw1000_ret_t dw1000_off();
float dw1000_tug_get_clockFrequencyOffset_cri_ppm();

dw1000_ret_t dw1000_set_config(const dwt_config_t* config);
const dwt_config_t* dw1000_get_config();
uint32_t dw1000_get_status();
uint32_t dw1000_get_state();
dw1000_ret_t dw1000_send(uint8_t *data, size_t length, uint8_t tx_mode);
const bool dw1000_is_transmitting();
float dw1000_get_last_rx_cfo_ppm();
uint64_t dw1000_get_last_rx_timestamp();
uint64_t dw1000_get_last_tx_timestamp();
uint64_t dw1000_get_cur_timestamp();

#endif