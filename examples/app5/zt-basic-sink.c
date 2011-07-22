
#include "contiki.h"
#include "net/rime.h"
#include "net/rime/mesh.h"

#include "leds.h"

#include <stdio.h>
#include <string.h>

#define MY_ADDR1 111
#define MY_ADDR2 111
#define CHANNEL 111

#define NODE_TABLE_SIZE 200
#define POLL_INTERVAL CLOCK_SECOND*15
#define MAX_RESENDS 5



PROCESS(zoundtracker_sink_process, "ZoundTracker Sink Process");
AUTOSTART_PROCESSES(&zoundtracker_sink_process);

//---------------------------------------------------------------------------------------------------------------------

typedef struct
{
	unsigned short addr1;
	unsigned short addr2;
	unsigned char empty;
} NodeTableEntry;

static NodeTableEntry nodeTable[NODE_TABLE_SIZE];


static struct mesh_conn zoundtracker_conn;


static unsigned char isReceived;

static int i;

static int resends;

//---------------------------------------------------------------------------------------------------------------------

static void sent(struct mesh_conn *c)
{
	printf("Packet sent.\n\n");
}

static void timedout(struct mesh_conn *c)
{
	printf("Packet timeouted. Resends = %d\n\n", resends+1);
	if (resends++ < MAX_RESENDS)
	{
		rimeaddr_t addr_send;
		addr_send.u8[0] = nodeTable[i].addr1;
		addr_send.u8[1] = nodeTable[i].addr2;
		packetbuf_copyfrom("poll", 4);
		
		mesh_send(&zoundtracker_conn, &addr_send);
	}
	else isReceived = 1;
}

static void received(struct mesh_conn *c, const rimeaddr_t *from, uint8_t hops)
{
	if (!memcmp(packetbuf_dataptr(), (void *)"hello", 5))
	{
	    leds_on(LEDS_GREEN);
		printf("Hello packet received from %d.%d\n\n", from->u8[0], from->u8[1]);
		int j, firstEmptyPos = -1;
		unsigned char exit = 0;
		for (j = 0; j < NODE_TABLE_SIZE && !exit; j++)
		{
			if (nodeTable[j].empty) firstEmptyPos = j;
			else if (nodeTable[j].addr1 == from->u8[0] &&
			  nodeTable[j].addr2 == from->u8[1])
				exit = 1;
		}
		if (firstEmptyPos == -1) printf("NODE TABLE COMPLETE!\n\n");
		else if (exit) printf("This node already exists in node table\n\n");
		else
		{
			nodeTable[firstEmptyPos].empty = 0;
			nodeTable[firstEmptyPos].addr1 = from->u8[0];
			nodeTable[firstEmptyPos].addr2 = from->u8[1];
		}
	}
	else
	{
	    leds_on(LEDS_RED);
	    leds_off(LEDS_YELLOW);
		printf("Packet received:\n From: %d.%d\n Data length: %d\n Data: %.*s\n Hops: %d\n\n",
			from->u8[0], from->u8[1], packetbuf_datalen(),
			packetbuf_datalen(), (char *)packetbuf_dataptr(), hops);
		isReceived = 1;
	}
}

const static struct mesh_callbacks zoundtracker_sink_callbacks = {received, sent, timedout};

//---------------------------------------------------------------------------------------------------------------------

PROCESS_THREAD(zoundtracker_sink_process, ev, data)
{
	PROCESS_EXITHANDLER(mesh_close(&zoundtracker_conn);)
	PROCESS_BEGIN();
	
	
	int j;
	for (j = 0; j < NODE_TABLE_SIZE; j++)
	{
		nodeTable[j].empty = 1;
	}
	
	rimeaddr_t my_addr;
	my_addr.u8[0] = MY_ADDR1;
	my_addr.u8[1] = MY_ADDR2;
	rimeaddr_set_node_addr(&my_addr);

	mesh_open(&zoundtracker_conn, CHANNEL, &zoundtracker_sink_callbacks);
	
	static struct etimer timer;
	etimer_set(&timer, POLL_INTERVAL);

	while (1)
	{
		//PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));
		while(!etimer_expired(&timer));
		for (i = 0; i < NODE_TABLE_SIZE; i++)
		{
			if (!nodeTable[i].empty)
			{
				rimeaddr_t addr_send;
				addr_send.u8[0] = nodeTable[i].addr1;
				addr_send.u8[1] = nodeTable[i].addr2;
				packetbuf_copyfrom((void *)"poll", 4);
				
				isReceived = 0;
				resends = 0;
				leds_off(LEDS_GREEN);
				leds_off(LEDS_RED);
				leds_on(LEDS_YELLOW);
				printf("Sending poll packet\n\n");
				mesh_send(&zoundtracker_conn, &addr_send);
				
				static struct etimer timer2;
				etimer_set(&timer2, CLOCK_SECOND*2);
				while (!isReceived && resends < MAX_RESENDS)
				{
					//PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer2));
					while (!etimer_expired(&timer2));
					resends++;
					leds_off(LEDS_GREEN);
					leds_off(LEDS_RED);
					leds_on(LEDS_YELLOW);
					printf("Sending poll packet\n\n");
					mesh_send(&zoundtracker_conn, &addr_send);
					etimer_reset(&timer2);
				}
				if (!isReceived)
				{
					printf("Answer packet not received. Entry %d of node table will be erased.\n\n", i);
					nodeTable[i].empty = 1;
				}
			}
		}
		etimer_reset(&timer);
	}
	PROCESS_END();
}
