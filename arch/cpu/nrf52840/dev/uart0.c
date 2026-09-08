/*
 * UARTE TX (EasyDMA) + RX single-byte DMA driver — FIXED
 *
 * - Uses modulo wrap (any buffer size)
 * - Splits EasyDMA TX transfers into device-safe chunks (<= UARTE_MAX_XFER)
 *
 * Note: UARTE/MAXCNT width is device-dependent; 255 is safe on most nRF52 chips.
 * See Nordic Q&A / errata for details. :contentReference[oaicite:1]{index=1}
 */

#include "contiki.h"
#include "nrf.h"
#include "nrf_uarte.h"
#include "nrf_gpio.h"
#include "dev/uart0.h"

#include <stdint.h>
#include <stdbool.h>

/*---------------------------------------------------------------------------*/
/* Application-visible configuration */
#define UART_TX_BUF_SIZE 4096u /* any size (not required to be power-of-two) */
#define UARTE_MAX_XFER   255u  /* Safe DMA chunk size (device-dependent) */

/*---------------------------------------------------------------------------*/
static int (*input_handler)(unsigned char c);

#define UART_INSTANCE NRF_UARTE0
#define TX_PIN  NRF_UART0_TX_PIN
#define RX_PIN  NRF_UART0_RX_PIN

/* TX ring buffer */
static uint8_t tx_buf[UART_TX_BUF_SIZE];
static volatile uint32_t tx_head = 0; /* next position to write */
static volatile uint32_t tx_tail = 0; /* next position to send */
static volatile bool tx_active = false; /* true while a DMA transfer is active */

/* RX single-byte DMA buffer */
static uint8_t rx_byte;

/*---------------------------------------------------------------------------*/
/* Helpers */
static inline bool tx_buffer_empty(void) { return tx_head == tx_tail; }
static inline bool tx_buffer_full(void)  { return (((tx_head + 1) % UART_TX_BUF_SIZE) == tx_tail); }

/* Critical-section helpers (only disable/enable the UARTE IRQ) */
static inline void uart_irq_disable(void) { NVIC_DisableIRQ(UARTE0_UART0_IRQn); }
static inline void uart_irq_enable(void)  { NVIC_EnableIRQ(UARTE0_UART0_IRQn); }

/*---------------------------------------------------------------------------*/
void
uart0_set_input(int (*input)(unsigned char c))
{
  input_handler = input;

  if (input_handler != NULL) {
    /* Configure RX single-byte DMA */
    UART_INSTANCE->RXD.PTR    = (uint32_t)&rx_byte;
    UART_INSTANCE->RXD.MAXCNT = 1;
    UART_INSTANCE->EVENTS_ENDRX = 0;
    UART_INSTANCE->EVENTS_RXSTARTED = 0;

    /* Enable ENDRX interrupt (single-byte RX) */
    UART_INSTANCE->INTENSET = UARTE_INTENSET_ENDRX_Msk;

    NVIC_ClearPendingIRQ(UARTE0_UART0_IRQn);
    NVIC_EnableIRQ(UARTE0_UART0_IRQn);

    /* Start RX */
    UART_INSTANCE->TASKS_STARTRX = 1;
  } else {
    /* Disable RX interrupts/tasks */
    UART_INSTANCE->INTENCLR = UARTE_INTENCLR_ENDRX_Msk;
    NVIC_DisableIRQ(UARTE0_UART0_IRQn);
    UART_INSTANCE->TASKS_STOPRX = 1;
  }
}

/*---------------------------------------------------------------------------*/
/**
 * uart0_write() - enqueue bytes into the TX ring buffer (non-blocking)
 * returns number of bytes actually enqueued.
 *
 * Splits DMA transfers into chunks of up to UARTE_MAX_XFER bytes.
 */
int
uart0_write(const unsigned char *data, int len)
{
  int written = 0;

  uart_irq_disable(); /* short critical section to avoid races with ISR */

  for (int i = 0; i < len; i++) {
    uint32_t next_head = (tx_head + 1) % UART_TX_BUF_SIZE;
    if (next_head == tx_tail) {
      /* buffer full, stop */
      break;
    }
    tx_buf[tx_head] = data[i];
    tx_head = next_head;
    written++;
  }

  /* If no DMA active, start one chunk from tail */
  if (!tx_active && !tx_buffer_empty()) {
    tx_active = true;

    /* number of bytes from tail to head (contiguous) */
    uint32_t contiguous;
    if (tx_head >= tx_tail) {
      contiguous = tx_head - tx_tail;
    } else {
      contiguous = UART_TX_BUF_SIZE - tx_tail;
    }

    /* Cap to the hardware-safe max chunk */
    uint32_t cnt = (contiguous > UARTE_MAX_XFER) ? UARTE_MAX_XFER : contiguous;

    /* Setup DMA pointer and length (cnt <= UARTE_MAX_XFER) */
    UART_INSTANCE->TXD.PTR    = (uint32_t)&tx_buf[tx_tail];
    UART_INSTANCE->TXD.MAXCNT = cnt;
    UART_INSTANCE->EVENTS_ENDTX = 0;

    /* Enable ENDTX interrupt and start transfer */
    UART_INSTANCE->INTENSET = UARTE_INTENSET_ENDTX_Msk;
    UART_INSTANCE->TASKS_STARTTX = 1;
  }

  uart_irq_enable(); /* exit critical section */
  return written;
}

/* Single-byte convenience wrapper */
void
uart0_writeb(unsigned char c)
{
  (void)uart0_write(&c, 1);
}

/*---------------------------------------------------------------------------*/
void
uart0_init(unsigned long ubr)
{
  /* Disable peripheral while configuring */
  UART_INSTANCE->ENABLE = UARTE_ENABLE_ENABLE_Disabled;

  /* Configure pins */
  nrf_gpio_cfg_output(TX_PIN);
  nrf_gpio_pin_set(TX_PIN);
  nrf_gpio_cfg_input(RX_PIN, NRF_GPIO_PIN_NOPULL);

  /* Baudrate (using your macro) */
  UART_INSTANCE->BAUDRATE = (uint32_t)UART0_CONF_BAUD_RATE;

  /* Parity excluded, HWFC disabled */
  UART_INSTANCE->CONFIG = (UARTE_CONFIG_PARITY_Excluded << UARTE_CONFIG_PARITY_Pos)
                         | (UARTE_CONFIG_HWFC_Disabled << UARTE_CONFIG_HWFC_Pos);

  /* Assign pins */
  UART_INSTANCE->PSEL.TXD = TX_PIN;
  UART_INSTANCE->PSEL.RXD = RX_PIN;

  /* Clear events and interrupts */
  UART_INSTANCE->EVENTS_ENDRX = 0;
  UART_INSTANCE->EVENTS_ENDTX = 0;
  UART_INSTANCE->EVENTS_RXSTARTED = 0;
  UART_INSTANCE->INTENCLR = 0xFFFFFFFF;

  /* Enable peripheral */
  UART_INSTANCE->ENABLE = UARTE_ENABLE_ENABLE_Enabled;

  /* Start TX task so TX pin outputs idle state */
  UART_INSTANCE->TASKS_STARTTX = 1;

  /* Prepare RX single-byte DMA but do not start until set_input called */
  rx_byte = 0;

  /* Init buf pointers */
  tx_head = tx_tail = 0;
  tx_active = false;

  NVIC_ClearPendingIRQ(UARTE0_UART0_IRQn);
  NVIC_EnableIRQ(UARTE0_UART0_IRQn);
}

/*---------------------------------------------------------------------------*/
/* UARTE IRQ: handle ENDRX (single-byte RX), ENDTX (TX chunk finished) */
void
UARTE0_UART0_IRQHandler(void)
{
  /* --- RX: single-byte completed (ENDRX) --- */
  if (UART_INSTANCE->EVENTS_ENDRX) {
    UART_INSTANCE->EVENTS_ENDRX = 0;

    if (input_handler) {
      /* RXD.AMOUNT will be 1 for single-byte DMA */
      input_handler(rx_byte);
    }

    /* Re-arm RX DMA: set PTR/MAXCNT and restart */
    UART_INSTANCE->RXD.PTR    = (uint32_t)&rx_byte;
    UART_INSTANCE->RXD.MAXCNT = 1;
    UART_INSTANCE->EVENTS_ENDRX = 0;
    UART_INSTANCE->TASKS_STARTRX = 1;
  }

  /* --- TX: DMA chunk finished (ENDTX) --- */
  if (UART_INSTANCE->EVENTS_ENDTX) {
    /* Clear event FIRST to avoid retrigger race */
    UART_INSTANCE->EVENTS_ENDTX = 0;

    /* How many bytes were transferred in the chunk (<= UARTE_MAX_XFER) */
    uint32_t sent = UART_INSTANCE->TXD.AMOUNT;
    if (sent > 0) {
      tx_tail = (tx_tail + sent) % UART_TX_BUF_SIZE;
    }

    /* If more data pending, start next contiguous chunk (cap to UARTE_MAX_XFER) */
    if (!tx_buffer_empty()) {
      uint32_t contiguous;
      if (tx_head >= tx_tail) {
        contiguous = tx_head - tx_tail;
      } else {
        contiguous = UART_TX_BUF_SIZE - tx_tail;
      }

      uint32_t cnt = (contiguous > UARTE_MAX_XFER) ? UARTE_MAX_XFER : contiguous;

      UART_INSTANCE->TXD.PTR    = (uint32_t)&tx_buf[tx_tail];
      UART_INSTANCE->TXD.MAXCNT = cnt;
      UART_INSTANCE->EVENTS_ENDTX = 0;
      UART_INSTANCE->TASKS_STARTTX = 1;
      /* leave ENDTX interrupt enabled */
    } else {
      /* Nothing left — mark idle and disable ENDTX interrupt */
      tx_active = false;
      UART_INSTANCE->INTENCLR = UARTE_INTENCLR_ENDTX_Msk;
      UART_INSTANCE->EVENTS_ENDTX = 0;
    }
  }
}

/*---------------------------------------------------------------------------*/
