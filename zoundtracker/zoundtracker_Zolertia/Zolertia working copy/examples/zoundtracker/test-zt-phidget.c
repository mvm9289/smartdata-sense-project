/*
 * Copyright (c) 2007, Swedish Institute of Computer Science.
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
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
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
 * This file is part of the Contiki operating system.
 *
 * $Id: test-phidgets.c,v 1.1 2010/08/27 12:51:41 joxe Exp $
 */

/**
 * \file
 *         An example of how to use the button and light sensor on
 *         the Z1 platform.
 * \author
 *         Joakim Eriksson <joakime@sics.se>
 */

#include <stdio.h>
#include "contiki.h"
#include "dev/button-sensor.h"
#include "dev/leds.h"
#include "dev/zt-phidget.h"

#define SAMPLE_TIMER CLOCK_SECOND/100
#define SAMPLE_NUMBER (8000*2)

/*---------------------------------------------------------------------------*/
PROCESS(test_button_process, "Test Button & Phidgets");
AUTOSTART_PROCESSES(&test_button_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(test_button_process, ev, data)
{
  static struct etimer et;
  static int soundval = 0;
  static unsigned int sampcont = 0;
  static unsigned long int holdbuf = 0;
  static unsigned long int resultbuf = 0;  
 
  PROCESS_BEGIN();
  SENSORS_ACTIVATE(ztalog);
  SENSORS_ACTIVATE(button_sensor);

  printf("Obtaining average\n");

  while(sampcont < SAMPLE_NUMBER) {
    etimer_set(&et, SAMPLE_TIMER/80);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    sampcont++;
    holdbuf += ztalog.value(PHIDGET3V_2);
  }

  resultbuf = holdbuf/SAMPLE_NUMBER;
  printf("Avg: %u\n", resultbuf);

  printf("Press the User Button\n");
  PROCESS_WAIT_EVENT_UNTIL(ev == sensors_event && data == &button_sensor);

  while(1) {
    soundval = ztalog.value(PHIDGET3V_2);
    soundval -= resultbuf;
    soundval *= soundval;
    printf("A7:%u\n", soundval);
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
