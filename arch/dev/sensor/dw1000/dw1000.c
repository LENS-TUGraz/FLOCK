#include "contiki.h"

#include "dw1000.h"
#include "dw1000-arch.h"
#include "dw1000-conf.h"
#include "dw1000-msg.h"
#include "dw1000-staggered-ranging.h"
#include "deca_device_api.h"
#include "deca_types.h"
#include "deca_regs.h"
#include "math.h"

#include <string.h>

/* Log configuration */
#include "sys/log.h"
#include "sys/node-id.h"
#define LOG_MODULE "DW1000"
#define LOG_LEVEL LOG_LEVEL_WARN

static uint64_t get_rx_timestamp();
struct rtimer dw1000_timer;
static uint32_t dw1000_status;

/* Declare callback functions */
static void rx_ok_cb(const dwt_cb_data_t *cb_data);
static void tx_ok_cb(const dwt_cb_data_t *cb_data);
static void rx_to_cb(const dwt_cb_data_t *cb_data);
static void rx_err_cb(const dwt_cb_data_t *cb_data);

/* Radio buffer */
static volatile bool tx_done = false;
static uint8_t rx_buffer[DW1000_BUFFER_SIZE]; // RX buffer

static volatile uint64_t rx_timestamp;
static uint16_t rx_pacc_nosat;
#if DW1000_WITH_NLOS
#include "sys/rtimer.h"
static dwt_rxdiag_t rx_diag;
static uint8_t acc[ACC_READ_STEP + 1];    // RX CIR accumulator buffer
static float rx_cir_abs[CIR_BUFFER_SIZE]; // RX CIR buffer
#endif
static float rx_cfo_ppm;

/* Radio variables */
static dw1000_mode_t dw1000_mode = DW1000_MODE_IDLE;
static size_t rx_frame_length;

/* Radio config */
static dwt_config_t dw1000_config = DW1000_CONFIG;


PROCESS(dw1000_process, "DW1000 process");

/* ------------------------------------------------------------- */
/* ------------------------------------------------------------- */
/* ------------------------------------------------------------- */
/* ------------------------------------------------------------- */
dw1000_ret_t 
dw1000_init() 
{
  LOG_DBG("init\n");
  dw1000_arch_init();

  DW1000_DEBUG_TX_INIT();
  DW1000_DEBUG_RX_INIT();
  
  /* check if device id can be read correctly */
  if(dwt_readdevid() == 0xDECA0130) {
    LOG_INFO("DW1000 found\n");
  } else {
    LOG_ERR("No DW1000 found\n");
    return DW1000_ERR;
  }

  dw1000_set_config(&dw1000_config);

  dwt_setsmarttxpower(0);
  dwt_write32bitreg(TX_POWER_ID, 0x85858585);

  dwt_settxantennadelay(DW1000_TX_ANT_DELAY);
  dwt_setrxantennadelay(DW1000_RX_ANT_DELAY);

  dwt_setleds(DWT_LEDS_ENABLE);

  dw1000_status = dwt_read32bitreg(SYS_STATUS_ID);
  if(dw1000_status & (SYS_STATUS_CLKPLL_LL | SYS_STATUS_RFPLL_LL)){
    LOG_WARN("Warning: CLKPLL or RFPLL losing lock. status=0x%08lX\n", (uint32_t) dw1000_status);
  }

  dwt_setinterrupt(SYS_STATUS_RXFCG | SYS_STATUS_TXFRS | SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR, 1);
  dwt_setcallbacks(&tx_ok_cb, &rx_ok_cb, &rx_to_cb, &rx_err_cb);
  LOG_DBG("status: 0x%08lX\n", dw1000_status);
  dw1000_mode = DW1000_MODE_IDLE;

  process_start(&dw1000_process, NULL);

  PT_INIT(&staggered_ranging_pt);

  dw1000_on();

  LOG_DBG("init done\n");
  return DW1000_OK;
}

dw1000_ret_t
dw1000_reset(){
  LOG_DBG("reset\n");
  dw1000_off();
  dw1000_arch_reset();
  dw1000_on();

  return DW1000_OK;
}

dw1000_ret_t
dw1000_on()
{
//  dw1000_off();
  
  if(dwt_rxenable(DWT_START_RX_IMMEDIATE) != DWT_SUCCESS) {
    LOG_ERR("Failed to enable RX\n");
    DW1000_DEBUG_RX_END();
    return DW1000_ERR;
  }
  DW1000_DEBUG_RX_START();

  dw1000_mode = DW1000_MODE_RX;
  return DW1000_OK;
}

dw1000_ret_t
dw1000_on_dly(uint32_t delay_us)
{
//  dw1000_off();
  uint64_t cur_ts = ((uint64_t) dwt_readsystimestamphi32()) << 8;
  uint64_t rx_on_ts = cur_ts + DW1000_US_TO_RADIO_TIME(delay_us);

  dwt_setdelayedtrxtime(rx_on_ts);
  if(dwt_rxenable(DWT_START_RX_DELAYED) != DWT_SUCCESS) {
    LOG_ERR("Failed to enable RX delayed, starting it immediate instead\n");
    dw1000_on();
    return DW1000_ERR;
  }
  DW1000_DEBUG_RX_START();

  dw1000_mode = DW1000_MODE_RX;
  return DW1000_OK;
}

dw1000_ret_t
dw1000_off()
{
  dwt_forcetrxoff();
  dwt_rxreset();
  DW1000_DEBUG_RX_END();

  dw1000_mode = DW1000_MODE_IDLE;
  return DW1000_OK;
}

dw1000_ret_t 
dw1000_set_config(const dwt_config_t* config)
{
  dw1000_off();
  memcpy(&dw1000_config, config, sizeof(dwt_config_t));

  dwt_configure(&dw1000_config);

  if(config->txPreambLength == DWT_PLEN_64){
    dwt_configurefor64plen(config->prf);
  }

  return DW1000_OK;
}

const dwt_config_t*
dw1000_get_config()
{
  return &dw1000_config;
}

uint32_t dw1000_get_status()
{
  dw1000_status = dwt_read32bitreg(SYS_STATUS_ID);
  return dw1000_status;
}

uint32_t dw1000_get_state()
{
  return dwt_read32bitreg(SYS_STATE_ID);
}

dw1000_ret_t
dw1000_send(uint8_t *data, size_t length, uint8_t tx_mode)
{
  LOG_DBG("send\n");
  tx_done = false;

  dw1000_off();

  dwt_writetxdata(length + 2, data, 0);
  dwt_writetxfctrl(length + 2, 0, 0);
  if(dwt_starttx(tx_mode) != DWT_SUCCESS) {
    LOG_ERR("Failed to start TX : %lu\n", dwt_readsystimestamphi32());
    dw1000_on();
    return DW1000_ERR;
  }
  DW1000_DEBUG_TX_START();

  LOG_DBG("send done\n");
  return DW1000_OK;
}

const bool dw1000_is_transmitting(){
  return !tx_done;
}

float 
dw1000_tug_get_clockFrequencyOffset_cri_ppm(){
  float mult;
  float res;

  if(dw1000_config.dataRate == DWT_BR_110K) 
    mult = FREQ_OFFSET_MULTIPLIER_110KB;
  else 
    mult = FREQ_OFFSET_MULTIPLIER;

  switch(dw1000_config.chan){
  case 1:
    mult = mult * HERTZ_TO_PPM_MULTIPLIER_CHAN_1;
    break;
  case 2:
    mult = mult * HERTZ_TO_PPM_MULTIPLIER_CHAN_2;
    break;
  case 3:
    mult = mult * HERTZ_TO_PPM_MULTIPLIER_CHAN_3;
    break;
  case 4:
    mult = mult * HERTZ_TO_PPM_MULTIPLIER_CHAN_2;
    break;
  case 5:
    mult = mult * HERTZ_TO_PPM_MULTIPLIER_CHAN_5;
    break;
  case 7:
    mult = mult * HERTZ_TO_PPM_MULTIPLIER_CHAN_5;
    break;
  default:
    mult = mult * HERTZ_TO_PPM_MULTIPLIER_CHAN_2;
    break;
  }

  res = -((float) dwt_readcarrierintegrator()) * mult;

  return res;
}

static uint64_t get_rx_timestamp()
{
  uint8_t ts_tab[5];
  uint64_t ts = 0;
  int i;
  
  dwt_readrxtimestamp(ts_tab);
  for(i = 4; i >= 0; i--){
    ts <<= 8;
    ts |= ts_tab[i];
  }
  return ts;
}

static uint64_t get_tx_timestamp()
{
  uint8_t ts_tab[5];
  uint64_t ts = 0;
  int i;
  
  dwt_readtxtimestamp(ts_tab);
  for(i = 4; i >= 0; i--){
    ts <<= 8;
    ts |= ts_tab[i];
  }
  return ts;
}

uint64_t
dw1000_get_last_rx_timestamp()
{
  return rx_timestamp;
}

uint64_t
dw1000_get_last_tx_timestamp()
{
  return get_tx_timestamp();
}

float
dw1000_get_last_rx_cfo_ppm()
{
  return rx_cfo_ppm;
}


#define DW1000_CIR_LEN_PRF16 992
#define DW1000_CIR_LEN_PRF64 1016

#if DW1000_WITH_NLOS
void
get_absolute_cir_samples(float* abs_cir, uint16_t start_idx, uint16_t cir_len)
{
  uint16_t chunk_len = 0;
  // printf("%s:%u    CIR STart %u Length %u \n",__FUNCTION__,__LINE__,start_idx,cir_len);
  uint16_t acc_len_bytes = (dw1000_config.prf == DWT_PRF_64M) ? (1016*4) : (992*4);

  int32_t a = 0; /* Real part */
  int32_t b = 0; /* Imaginary part */
  uint16_t cir_sample_counter = 0;
  uint16_t max_bytes = ((start_idx + cir_len) * 4 < acc_len_bytes) ? ((start_idx + cir_len) * 4) : acc_len_bytes;

  for(uint16_t j = start_idx * 4; j < max_bytes; j = j + ACC_READ_STEP) {
    /* Select the number of bytes to read from the accummulator */
    chunk_len = ACC_READ_STEP;
    if (j + ACC_READ_STEP > max_bytes) {
      chunk_len = max_bytes - j;
    }
    dwt_readaccdata(acc, chunk_len + 1, j);

    /* Store the bytes read as complex numbers */
    /* Ignore dummy octet by starting with k=1 */
    for(int k = 1; k < chunk_len + 1; k = k + 4) {
      a = (int32_t) (int16_t) (((acc[k + 1] & 0x00FF) << 8) | (acc[k] & 0x00FF));
      b = (int32_t) (int16_t) (((acc[k + 3] & 0x00FF) << 8) | (acc[k + 2] & 0x00FF));

      float abs_val = sqrt((float) (a*a + b*b));
      abs_cir[cir_sample_counter] = abs_val;
      cir_sample_counter++;
    }
  }
}
#endif

/* ------------------------------------------------------------- */
/* ------------------------------------------------------------- */
/* Callback functions */
/* ------------------------------------------------------------- */
/* ------------------------------------------------------------- */
static void 
rx_ok_cb(const dwt_cb_data_t *cb_data)
{
  DW1000_DEBUG_RX_END();
  LOG_DBG("RX OK\n");
  rx_frame_length = cb_data->datalength;
  if(rx_frame_length > sizeof(rx_buffer)){
    LOG_ERR("RX buffer overflow\n");
    return;
  }

  dwt_readrxdata(rx_buffer, rx_frame_length, 0);

  rx_pacc_nosat = dwt_read16bitoffsetreg(0x27, 0x28);
  rx_cfo_ppm = dw1000_tug_get_clockFrequencyOffset_cri_ppm();
  rx_timestamp = get_rx_timestamp();

#if DW1000_WITH_NLOS
  uint16_t fp_idx = (rx_diag.firstPath / 64.0);
//  LOG_INFO("firstPath: %u\n", fp_idx);
  rtimer_clock_t start = RTIMER_NOW();
  get_absolute_cir_samples(rx_cir_abs, fp_idx - CIR_BEFORE_FP, CIR_BEFORE_FP + CIR_AFTER_FP);
  dwt_readdiagnostics(&rx_diag);
  rtimer_clock_t end = RTIMER_NOW();
  LOG_WARN("CIR extraction time: %lu us\n", RTIMERTICKS_TO_US(end - start));
#endif
  process_poll(&dw1000_process);

//  dwt_rxreset();
  dw1000_on();
}

static void 
tx_ok_cb(const dwt_cb_data_t *cb_data)
{
  DW1000_DEBUG_TX_END();
  tx_done = true;
  LOG_DBG("TX OK\n");
//  dwt_rxreset();
  dw1000_on();
}

static void 
rx_to_cb(const dwt_cb_data_t *cb_data)
{
  DW1000_DEBUG_RX_END();
  LOG_DBG("RX TO\n");
//  dwt_rxreset();
  dw1000_on();
}

static void 
rx_err_cb(const dwt_cb_data_t *cb_data)
{
  DW1000_DEBUG_RX_END();
  LOG_DBG("RX ERR\n");
//  dwt_rxreset();
  dw1000_on();
}

/* ------------------------------------------------------------- */
/* ------------------------------------------------------------- */
/* DW1000 main process */
/* ------------------------------------------------------------- */
/* ------------------------------------------------------------- */
PROCESS_THREAD(dw1000_process, ev, data)
{
  PROCESS_BEGIN();
  LOG_DBG("process started\r\n");



  while(1){
    PROCESS_YIELD_UNTIL(ev == PROCESS_EVENT_POLL);

    dw1000_parse_msg(rx_buffer, rx_frame_length);

    LOG_DBG("RX frame length: %u\n", rx_frame_length);
    LOG_DBG("RX frame: ");
    for(size_t i = 0; i < rx_frame_length; i++){
      LOG_DBG_("%02X ", rx_buffer[i]);
    }
    LOG_DBG_("\n");
  }

  PROCESS_END();
}