#include "contiki.h"
#include "contiki-net.h"
#include "dev/leds.h"

#include <stdio.h>
#include <string.h>

static struct psock ps;
static char buffer[50];

static struct uip_conn *client_conn;
static uip_ipaddr_t ipaddr;


static PT_THREAD(handle_connection(struct psock *p))
{
  PSOCK_BEGIN(p);
  
  /*PSOCK_SEND_STR(p, "Welcome, please type something and press return.\n");
  PSOCK_READTO(p, '\n');*/
  PSOCK_SEND_STR(p, "Hello\n");
  
  /*PSOCK_SEND_STR(p, "Got the following data: ");
  PSOCK_SEND(p, buffer, PSOCK_DATALEN(p));
  PSOCK_SEND_STR(p, "Good bye!\r\n");*/

  PSOCK_CLOSE(p);
  
  PSOCK_END(p);
}

PROCESS(example_psock_client_process, "Example protosocket client");
AUTOSTART_PROCESSES(&example_psock_client_process);

PROCESS_THREAD(example_psock_client_process, ev, data)
{
  PROCESS_BEGIN();
  
  /*tcp_listen(HTONS(12345));*/
  uip_ipaddr(&ipaddr, 172,16,5,0);


  while(1) 
  {
    uip_connect(&ipaddr, HTONS(80)); 
    
    PROCESS_WAIT_EVENT_UNTIL(ev == tcpip_event);
    
    if(uip_connected()) 
    {
      PSOCK_INIT(&ps, buffer, sizeof(buffer));
      
      while(!(uip_aborted() || uip_closed() || uip_timedout())) 
      {
        handle_connection(&ps);
        PROCESS_WAIT_EVENT_UNTIL(ev == tcpip_event);
      }
    }
  }
  
  PROCESS_END();
}
