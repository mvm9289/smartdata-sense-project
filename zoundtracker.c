
#include "contiki.h"
#include "net/rime.h"
#include "net/rime/mesh.h"
#include "dev/leds.h" /******************/
#include "cfs/cfs.h"

#include <stdio.h>

#define MY_ADDR1 1
#define MY_ADDR2 121
#define SINK_ADDR1 111
#define SINK_ADDR2 111
#define CHANNEL 111
#define MAX_RESENDS 3
#define BUFF_SIZE 64

static rimeaddr_t sink_addr;
static struct mesh_conn zoundtracker_conn;
int resends;



PROCESS(zoundtracker_process, "ZoundTracker Process");
AUTOSTART_PROCESSES(&zoundtracker_process);



static void sent(struct mesh_conn *c) {}

static void timedout(struct mesh_conn *c)
{
	if (resends++ < MAX_RESENDS) mesh_send(&zoundtracker_conn, &sink_addr);
}

static void received(struct mesh_conn *c, const rimeaddr_t *from, uint8_t hops) {}

const static struct mesh_callbacks zoundtracker_callbacks = {received, sent, timedout};



PROCESS_THREAD(zoundtracker_process, ev, data)
{
	static struct etimer timer;
	static unsigned char buff_write[BUFF_SIZE];
	static unsigned char buff_read[BUFF_SIZE];
	static int fd;
	
	PROCESS_EXITHANDLER(mesh_close(&zoundtracker_conn);)
	PROCESS_BEGIN();
	
	int i;
	for (i = 0; i < BUFF_SIZE; i++) {
		buff_write[i] = 'a';
	}
	buff_write[BUFF_SIZE-1]=0;
	
	rimeaddr_t my_addr;
	my_addr.u8[0] = MY_ADDR1;
	my_addr.u8[1] = MY_ADDR2;
	rimeaddr_set_node_addr(&my_addr);
	
	sink_addr.u8[0] = SINK_ADDR1;
	sink_addr.u8[1] = SINK_ADDR2;

	mesh_open(&zoundtracker_conn, CHANNEL, &zoundtracker_callbacks);
	
	etimer_set(&timer, CLOCK_SECOND*10);
	
	leds_init(); /******************/

	while (1)
	{
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));
		leds_green(LEDS_ON); /******************/
		etimer_reset(&timer);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));
		leds_green(LEDS_OFF); /******************/
		etimer_reset(&timer);
		
		cfs_remove("cfs_file");	
		
		// Writing to 'cfs_file'
		if ((fd = cfs_open("cfs_file", CFS_WRITE)) != -1) {
			if (BUFF_SIZE == cfs_write(fd, buff_write, BUFF_SIZE)) {
				leds_red(LEDS_ON);
				cfs_close(fd);
			}
			else packetbuf_copyfrom("Error: cfs_write", 16);
		}
		else packetbuf_copyfrom("Error: cfs_open_write", 21);
		
		
		// Reading from 'cfs_file'
		if ((fd = cfs_open("cfs_file", CFS_READ)) != -1) {
			if (BUFF_SIZE == cfs_read(fd, buff_read, BUFF_SIZE)) {
				leds_red(LEDS_OFF);
				packetbuf_copyfrom(buff_read, BUFF_SIZE);
				cfs_close(fd);
			}
			else packetbuf_copyfrom("Error: cfs_read", 15);
		}
		else packetbuf_copyfrom("Error: cfs_open_read", 20);	
	
		resends = 0;
		
		sink_addr.u8[0] = SINK_ADDR1;
		sink_addr.u8[1] = SINK_ADDR2;
		mesh_send(&zoundtracker_conn, &sink_addr);
	}
	PROCESS_END();
}
