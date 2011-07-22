#include "contiki.h"
#include "contiki-net.h"

PROCESS(example_udp_server_process, "Example UDP server process");
AUTOSTART_PROCESSES(&example_udp_server_process);

PROCESS_THREAD(example_udp_server_process, ev, data)
{
  static struct uip_udp_conn *c;
  
  PROCESS_BEGIN();
  
  c = udp_new(NULL, UIP_HTONS(0)), NULL);
  //udp_bind(c, UIP_HTONS(80));
  
  while (1)
  {
    PROCESS_WAIT_EVENT_UNTIL(ev == tcpip_event);  
    uip_udp_packet_send(c, "Bye", 3); 
  }
  
  PROCESS_END();
}
