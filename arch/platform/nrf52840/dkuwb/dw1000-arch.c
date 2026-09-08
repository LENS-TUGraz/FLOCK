/*---------------------------------------------------------------------------*/
#include "nrf.h"
#include "nrf_drv_spi.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
/*--------------------------------------------------------------------------*/
#include "contiki.h"
#include "sys/clock.h"
#include "sys/log.h"
/*---------------------------------------------------------------------------*/
#include "leds.h"
/*---------------------------------------------------------------------------*/
#include "deca_device_api.h"
#include "deca_types.h"
#include "deca_regs.h"
/*---------------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>
/*---------------------------------------------------------------------------*/
#include "dw1000-arch.h"

#include "sys/log.h"
#define LOG_MODULE "DW1000 ARCH"
#define LOG_LEVEL LOG_LEVEL_WARN



/*---------------------------------------------------------------------------*/
#define NRF_DRV_SPI_DEFAULT_CONFIG_2M(id)                       \
{                                                            \
    .sck_pin      = SPI_CLK_PIN,      \
    .mosi_pin     = SPI_MOSI_PIN,     \
    .miso_pin     = SPI_MISO_PIN,     \
    .ss_pin       = SPI_CS_PIN,                \
    .irq_priority = NRFX_SPIM_DEFAULT_CONFIG_IRQ_PRIORITY, \
    .orc          = 0xFF,                                    \
    .frequency    = NRF_DRV_SPI_FREQ_2M,                     \
    .mode         = NRF_DRV_SPI_MODE_0,                      \
    .bit_order    = NRF_DRV_SPI_BIT_ORDER_MSB_FIRST,         \
}
#define NRF_DRV_SPI_DEFAULT_CONFIG_8M(id)                       \
{                                                            \
    .sck_pin      = SPI_CLK_PIN,      \
    .mosi_pin     = SPI_MOSI_PIN,     \
    .miso_pin     = SPI_MISO_PIN,     \
    .ss_pin       = SPI_CS_PIN,                \
    .irq_priority = NRFX_SPIM_DEFAULT_CONFIG_IRQ_PRIORITY, \
    .orc          = 0xFF,                                    \
    .frequency    = NRF_DRV_SPI_FREQ_8M,                     \
    .mode         = NRF_DRV_SPI_MODE_0,                      \
    .bit_order    = NRF_DRV_SPI_BIT_ORDER_MSB_FIRST,         \
}

static const nrf_drv_spi_t spi = NRF_DRV_SPI_INSTANCE(SPI_INSTANCE);
static volatile bool spi_xfer_done;
static volatile int dw1000_irqn_status;
/*---------------------------------------------------------------------------*/
/* Forward declarations */
static void dw1000_spi_init_slow_rate(void);
static void dw1000_spi_init_fast_rate(void);
/*---------------------------------------------------------------------------*/
/* DW1000 interrupt handler */

void spi_event_handler(nrf_drv_spi_evt_t const * p_event, void * p_context)
{
  spi_xfer_done = true;
}

static void
dw1000_irq_handler(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
  dw1000_irqn_status = 0; // we are in the interrupt handler, we set the
  // interrupt status to 'disabled' so that the
  // SPI functions do not disable/enable it in vain
  // (which is slow on this platform as it requires
  // a function call)
  do {
    dwt_isr();    
  } while(nrfx_gpiote_in_is_set(DW1000_IRQ_PIN) == true);
  dw1000_irqn_status = 1; // Marking it 'enabled' again
}
/*---------------------------------------------------------------------------*/
int
dw1000_disable_interrupt(void)
{
  if(dw1000_irqn_status != 0) {
    nrfx_gpiote_in_event_disable(DW1000_IRQ_PIN);
    dw1000_irqn_status = 0;
    return 1; // previous status was 'enabled'
  }
  else {
    return 0; // previous status was 'disabled'
  }
}
/*---------------------------------------------------------------------------*/
void
dw1000_enable_interrupt(int previous_irqn_status)
{
  if(previous_irqn_status != 0) {
    nrfx_gpiote_in_event_enable(DW1000_IRQ_PIN, true);
    dw1000_irqn_status = 1;
  }
}
/*---------------------------------------------------------------------------*/
static void
dw1000_spi_init_slow_rate(void)
{
  nrf_drv_spi_config_t  spi_config = NRF_DRV_SPI_DEFAULT_CONFIG_2M(SPI_INSTANCE);
	spi_config.ss_pin = SPI_CS_PIN;
	APP_ERROR_CHECK( nrf_drv_spi_init(&spi, &spi_config, spi_event_handler, NULL) );
	nrf_delay_ms(2);	
}
/*---------------------------------------------------------------------------*/
static void
dw1000_spi_init_fast_rate(void)
{
  nrf_drv_spi_uninit(&spi);
	nrf_drv_spi_config_t  spi_config = NRF_DRV_SPI_DEFAULT_CONFIG_8M(SPI_INSTANCE);
	spi_config.ss_pin = SPI_CS_PIN;
	APP_ERROR_CHECK( nrf_drv_spi_init(&spi, &spi_config, spi_event_handler,NULL) );
	nrf_delay_ms(2);	
}
/*---------------------------------------------------------------------------*/
void
dw1000_spi_open(void)
{

}
/*---------------------------------------------------------------------------*/
void
dw1000_spi_close(void)
{
}
/*---------------------------------------------------------------------------*/
int
dw1000_spi_read(uint16 hdrlen, const uint8 *hdrbuf, uint32 len, uint8 *buf)
{
  uint8 * p1;
  uint32 idatalength=0;
  idatalength= hdrlen + len;
  uint8 idatabuf[idatalength];
  uint8 itempbuf[idatalength];
  memset(idatabuf, 0, idatalength);
  memset(itempbuf, 0, idatalength);	
  p1=idatabuf;	
  memcpy(p1,hdrbuf, hdrlen);
  p1 += hdrlen;
  memset(p1,0x00,len);
  spi_xfer_done = false;
  nrf_drv_spi_transfer(&spi, idatabuf, idatalength, itempbuf, idatalength);
  while(!spi_xfer_done){}
  p1=itempbuf + hdrlen;
  memcpy(buf, p1, len);
  return 0;
}
/*---------------------------------------------------------------------------*/
int
dw1000_spi_write(uint16 hdrlen, const uint8 *hdrbuf, uint32 len, const uint8 *buf)
{
  uint8 * p1;
  uint32 idatalength=0;
  idatalength= hdrlen + len;

  uint8 idatabuf[idatalength];
  uint8 itempbuf[idatalength];
  memset(idatabuf, 0, idatalength);
  memset(itempbuf, 0, idatalength);		 
  
  p1=idatabuf;	
  memcpy(p1,hdrbuf, hdrlen);
  p1 += hdrlen;
  memcpy(p1,buf,len);
  
  spi_xfer_done = false;
  nrf_drv_spi_transfer(&spi, idatabuf, idatalength, itempbuf, idatalength);
  while(!spi_xfer_done){}
  return 0;
}
/*---------------------------------------------------------------------------*/
void
dw1000_arch_reset()
{
  LOG_DBG("reset\n");
  nrf_gpio_cfg_output(DW1000_RST);   
  nrf_gpio_pin_clear(DW1000_RST);  
  nrf_delay_ms(2); 
  nrf_gpio_pin_set(DW1000_RST);  
  nrf_delay_ms(50); 
  nrf_gpio_cfg_input(DW1000_RST, NRF_GPIO_PIN_NOPULL); 
  nrf_delay_ms(2);
  LOG_DBG("reset done\n");
}
/*---------------------------------------------------------------------------*/
/* Note that after calling this function you need to wait 5ms for XTAL to 
 * start and stabilise (or wait for PLL lock IRQ status bit: in SLOW SPI mode)
 */
void dw1000_arch_wakeup_nowait() {

}


void dw1000_arch_init() {
  LOG_DBG("init\n");
  nrfx_gpiote_init();

  dw1000_spi_init_slow_rate();
  nrf_delay_ms(1000);
  dw1000_arch_reset();
  nrf_delay_ms(1000);

  if(dwt_initialise(DWT_LOADUCODE) != DWT_SUCCESS) {
    LOG_ERR("Failed to initialise DW1000\n");
    return;
  }

  dw1000_spi_init_fast_rate();

  /* initialize interrupt */
  nrfx_gpiote_in_config_t in_config = NRFX_GPIOTE_CONFIG_IN_SENSE_LOTOHI(true);
  in_config.pull = NRF_GPIO_PIN_PULLDOWN;
  nrfx_gpiote_in_init(DW1000_IRQ_PIN, &in_config, dw1000_irq_handler);
  nrfx_gpiote_in_event_enable(DW1000_IRQ_PIN, true);
  dw1000_irqn_status = 1;

  LOG_DBG("init done\n");
}


