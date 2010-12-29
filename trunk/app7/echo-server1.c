#include "contiki.h"
#include "contiki-net.h"
#include "dev/leds.h"

#include <stdio.h>
#include <string.h>

/* uIP variables */
static uip_ipaddr_t ipaddr;
static struct uip_udp_conn *server_conn;

PROCESS(example_psock_server_process, "Example protosocket server");
AUTOSTART_PROCESSES(&example_psock_server_process);

/* Process definition */
PROCESS_THREAD(example_psock_server_process, ev, data)
{
  PROCESS_BEGIN();
  
  /* tcp_listen(UIP_HTONS(80)); */
  server_conn = udp_new(NULL, UIP_HTONS(0), NULL);
  udp_bind(server_conn, UIP_HTONS(80));
  
  while(1) 
  {
    PROCESS_WAIT_EVENT_UNTIL(ev == tcpip_event);
    printf("server connected\n");
  }
  
  PROCESS_END();
}
