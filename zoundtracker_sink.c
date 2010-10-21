
#include "contiki.h"
#include "net/rime.h"
#include "net/rime/mesh.h"
#include "dev/leds.h" /******************/

#include <stdio.h>

#define MY_ADDR1 111
#define MY_ADDR2 111
#define CHANNEL 111

static struct mesh_conn zoundtracker_conn;



PROCESS(zoundtracker_sink_process, "ZoundTracker Sink Process");
AUTOSTART_PROCESSES(&zoundtracker_sink_process);



static void sent(struct mesh_conn *c) {}

static void timedout(struct mesh_conn *c) {}

static void received(struct mesh_conn *c, const rimeaddr_t *from, uint8_t hops)
{
	printf("Packet received:\n From: %d.%d\n Data length: %d\n Data: %.*s\n Hops: %d\n",
		from->u8[0], from->u8[1], packetbuf_datalen(),
		packetbuf_datalen(), (char *)packetbuf_dataptr(), hops);
}

const static struct mesh_callbacks zoundtracker_sink_callbacks = {received, sent, timedout};



PROCESS_THREAD(zoundtracker_sink_process, ev, data)
{
	PROCESS_EXITHANDLER(mesh_close(&zoundtracker_conn);)
	PROCESS_BEGIN();
	
	rimeaddr_t my_addr;
	my_addr.u8[0] = MY_ADDR1;
	my_addr.u8[1] = MY_ADDR2;
	rimeaddr_set_node_addr(&my_addr);

	mesh_open(&zoundtracker_conn, CHANNEL, &zoundtracker_sink_callbacks);
	
	static struct etimer timer;
	etimer_set(&timer, CLOCK_SECOND);
	
	leds_init(); /******************/

	while (1)
	{
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));
		leds_green(LEDS_ON); /******************/
		etimer_reset(&timer);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));
		leds_green(LEDS_OFF); /******************/
		etimer_reset(&timer);
	}
	PROCESS_END();
}
