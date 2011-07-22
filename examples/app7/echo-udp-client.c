#include "contiki.h"
#include "contiki-net.h"

static struct etimer timer;

PROCESS(example_udp_client_process, "Example UDP client process");
AUTOSTART_PROCESSES(&example_udp_client_process);

PROCESS_THREAD(example_udp_client_process, ev, data)
{
  static struct uip_udp_conn *c;
  
  PROCESS_BEGIN();
  
  c = udp_broadcast_new(UIP_HTONS(80)), NULL);
  
  while (1)
  {
    etimer_set(&timer, CLOCK_SECOND);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));
    
    tcpip_poll_udp(c);
    PROCESS_WAIT_EVENT_UNTIL(ev == tcpip_event);
    
    uip_/*udp_packet_*/send(c, "Hello", 5);
  }
  
  PROCESS_END();
}
