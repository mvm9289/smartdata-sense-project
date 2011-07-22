#include "contiki.h"
#include "contiki-net.h"
#include "dev/leds.h"

#include <stdio.h>
#include <string.h>

static struct psock ps;
static char buffer[50];

static PT_THREAD(handle_connection(struct psock *p))
{
  PSOCK_BEGIN(p);
  
  PSOCK_SEND_STR(p, "Welcome, please type something and press return.\n");
  PSOCK_READTO(p, '\n');
  
  PSOCK_SEND_STR(p, "Got the following data: ");
  PSOCK_SEND(p, buffer, PSOCK_DATALEN(p));
  PSOCK_SEND_STR(p, "Good bye!\r\n");

  PSOCK_CLOSE(p);
  
  PSOCK_END(p);
}

PROCESS(example_psock_server_process, "Example protosocket server");
AUTOSTART_PROCESSES(&example_psock_server_process);

PROCESS_THREAD(example_psock_server_process, ev, data)
{
  PROCESS_BEGIN();
  
  tcp_listen(HTONS(80));

  while(1) 
  {
    PROCESS_WAIT_EVENT_UNTIL(ev == tcpip_event);

    if(uip_connected()) 
    {
      PSOCK_INIT(&ps, buffer, sizeof(buffer));
      
      while(!(uip_aborted() || uip_closed() || uip_timedout())) 
      {
        PROCESS_WAIT_EVENT_UNTIL(ev == tcpip_event);
        handle_connection(&ps);
      }
    }
  }
  
  PROCESS_END();
}
