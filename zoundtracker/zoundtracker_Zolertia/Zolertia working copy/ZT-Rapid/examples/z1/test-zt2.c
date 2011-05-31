/*
 * Copyright (c) 2011, Zolertia(TM) is a trademark of Advancare,SL
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
 *
 */

/**
 * \file
 *         A quick program for testing the ZoundTracker driver in the Z1 platform
 * \author
 *         Enric M. Calvo <ecalvo@zolertia.com>
 *         Antonio Linan, Zolertia <alinan@zolertia.com>
 */

#include <stdio.h>
#include "contiki.h"
#include "dev/i2cmaster.h"
#include "dev/zt.h"


#if 1
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif


#if 0
#define PRINTFDEBUG(...) printf(__VA_ARGS__)
#else
#define PRINTFDEBUG(...)
#endif

#define ZT_READ_INTERVAL CLOCK_SECOND/4
#define ZT_STARTUP_DELAY CLOCK_SECOND*10

PROCESS(temp_process, "Test ZT process");
AUTOSTART_PROCESSES(&temp_process);
/*---------------------------------------------------------------------------*/
static struct etimer et;
static struct etimer ew;

PROCESS_THREAD(temp_process, ev, data)
{
  PROCESS_BEGIN();

  int16_t valBuff[] = {0x00, 0x00};

  etimer_set(&ew, ZT_STARTUP_DELAY);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&ew));

  zt_init();

  while(1) {
    zt_write_reg (0xB0, 0xC0FE);
    zt_write_reg (0x20, 0xC0FE);

    etimer_set(&et, ZT_READ_INTERVAL/3);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    zt_read_spl_raw_X(&valBuff);

    PRINTF("Raw = 0x%04x 0x%04x\n", valBuff[0], valBuff[1]);

  }
  PROCESS_END();
}
