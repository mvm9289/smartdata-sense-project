#include "contiki.h"
#include "contiki-net.h"
#include "dev/leds.h"

#include <stdio.h>
#include <string.h>

static struct uip_udp_conn *client_conn;
static uip_ipaddr_t ipaddr;


PROCESS(example_psock_client_process, "Example protosocket client");
AUTOSTART_PROCESSES(&example_psock_client_process);

PROCESS_THREAD(example_psock_client_process, ev, data)
{
  PROCESS_BEGIN();
  
  /* Connecting to the server */
  uip_ipaddr(&ipaddr, 172,16,6,1);
  client_conn = udp_new(&ipaddr, UIP_HTONS(80), NULL);
  
  if (client_conn == NULL) printf("connection fail");
  
  PROCESS_END();
}
