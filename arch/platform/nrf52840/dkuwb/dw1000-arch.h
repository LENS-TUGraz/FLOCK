/*
 * Copyright (c) 2018, University of Trento, Italy
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``as-is'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
/**
 * \file
 *      Platform Dependent DW1000 Driver Header File
 *
 */

#ifndef _DW1000_ARCH_H_
#define _DW1000_ARCH_H_
/*---------------------------------------------------------------------------*/
#include "contiki.h"
#include "deca_types.h"
#include "nrf.h"

void dw1000_arch_init();
void dw1000_arch_reset();
int dw1000_spi_write(uint16 hdrlen, const uint8 *hdrbuf, uint32 len, const uint8 *buf);
int dw1000_spi_read(uint16 hdrlen, const uint8 *hdrbuf, uint32 len, uint8 *buf);
void dw1000_enable_interrupt(int previous_irqn_status);
int dw1000_disable_interrupt(void);
/*---------------------------------------------------------------------------*/
/* Platform-specific PIN definitions */

#define SPI_INSTANCE   1
#define SPI_MISO_PIN   NRF_GPIO_PIN_MAP(1, 14)
#define SPI_MOSI_PIN   NRF_GPIO_PIN_MAP(1, 13)
#define SPI_CLK_PIN    NRF_GPIO_PIN_MAP(1, 15)
#define SPI_CS_PIN     NRF_GPIO_PIN_MAP(1, 12)
#define DW1000_IRQ_PIN NRF_GPIO_PIN_MAP(1, 11)
#define DW1000_RST     NRF_GPIO_PIN_MAP(0, 03)
#define DW1000_GPIO0   NRF_GPIO_PIN_MAP(1, 8)
#define DW1000_GPIO1   NRF_GPIO_PIN_MAP(1, 07)
#define DW1000_GPIO2   NRF_GPIO_PIN_MAP(1, 06)
#define DW1000_GPIO3   NRF_GPIO_PIN_MAP(1, 05)
#define DW1000_GPIO4   NRF_GPIO_PIN_MAP(1, 04)
#define DW1000_GPIO5   NRF_GPIO_PIN_MAP(1, 03)


/*---------------------------------------------------------------------------*/
/* Platform-specific bindings for the DW1000 driver */
#define writetospi(cnt, header, length, buffer) dw1000_spi_write(cnt, header, length, buffer)
#define readfromspi(cnt, header, length, buffer) dw1000_spi_read(cnt, header, length, buffer)
#define decamutexon() dw1000_disable_interrupt()
#define decamutexoff(stat) dw1000_enable_interrupt(stat)
#define deca_sleep(t) clock_delay_usec(t * 1000)
/*---------------------------------------------------------------------------*/
#endif /* _DW1000_ARCH_H_ */
