#include "contiki.h"
#include "net/rime.h"
#include "net/rime/mesh.h"
#include "dev/leds.h"
#include "cfs/cfs.h"
#include <stdio.h>
#include<string.h>

#define MY_ADDR1 1
#define MY_ADDR2 121
#define SINK_ADDR1 111
#define SINK_ADDR2 111
#define CHANNEL 111
#define MAX_RESENDS 3
#define BUFF_SIZE 64
#define TIME CLOCK_SECOND*10
#define MSG_SIZE 80


// Global variables
static rimeaddr_t sink_addr;
static struct mesh_conn zoundtracker_conn;
int resends, file_readed, bytes_readed;
static int fd, nfile;
static unsigned char filename[32];
static unsigned char buff_read[BUFF_SIZE];


// Functions
static void sent(struct mesh_conn *c) {} ////////////////////////////////////////////////////////////////////Añadir nfile++ y todo el control que ello conlleva

static void timedout(struct mesh_conn *c) {
	if (resends++ < MAX_RESENDS) 
	{
		sink_addr.u8[0] = SINK_ADDR1;
		sink_addr.u8[1] = SINK_ADDR2;
		mesh_send(&zoundtracker_conn, &sink_addr);
	}
}

static void received(struct mesh_conn *c, const rimeaddr_t *from, uint8_t hops) {
	// Basestation request received
	if (!strcmp((char *)packetbuf_dataptr(), "poll")) {
		printf("Poll received\n\n");
		//process_poll(PROCESS_CURRENT());
			// Sending the current file to the Basestation 
	
	// Reading the entire file	
	fd = cfs_open(filename, CFS_READ);
	if (fd < 0) printf("Error opening file\n\n");
	
	file_readed = 0;
	while(file_readed == 0) {		
		bytes_readed = cfs_read(fd, buff_read, MSG_SIZE);
		
		if(bytes_readed > 0) {
			packetbuf_copyfrom(buff_read, bytes_readed);
			
			resends = 0;			
			sink_addr.u8[0] = SINK_ADDR1;
			sink_addr.u8[1] = SINK_ADDR2;
			mesh_send(&zoundtracker_conn, &sink_addr);			

		}
		else {
			// Current file sended			
			file_readed = 1;			
			cfs_close(fd);

			// Updating the current file
			nfile++;
		}
	}	
	printf("File sent\n\n");
	}
}

const static struct mesh_callbacks zoundtracker_callbacks = {received, sent, timedout};

static void poll_handler() {	
	// Sending the current file to the Basestation 
	
	// Reading the entire file	
	fd = cfs_open(filename, CFS_READ);
	if (fd < 0) printf("Error opening file\n\n");
	
	file_readed = 0;
	while(file_readed == 0) {		
		bytes_readed = cfs_read(fd, buff_read, MSG_SIZE);
		
		if(bytes_readed > 0) {
			packetbuf_copyfrom(buff_read, bytes_readed);
			
			resends = 0;			
			sink_addr.u8[0] = SINK_ADDR1;
			sink_addr.u8[1] = SINK_ADDR2;
			mesh_send(&zoundtracker_conn, &sink_addr);			

		}
		else {
			// Current file sended			
			file_readed = 1;			
			cfs_close(fd);

			// Updating the current file
			nfile++;
		}
	}	
	printf("File sent\n\n");
}


PROCESS(zoundtracker_process, "ZoundTracker Process");
AUTOSTART_PROCESSES(&zoundtracker_process);

PROCESS_THREAD(zoundtracker_process, ev, data)
{
	static struct etimer timer;

	PROCESS_POLLHANDLER(poll_handler();)	
	PROCESS_EXITHANDLER(mesh_close(&zoundtracker_conn);)
	PROCESS_BEGIN();
	
	rimeaddr_t my_addr;
	my_addr.u8[0] = MY_ADDR1;
	my_addr.u8[1] = MY_ADDR2;
	rimeaddr_set_node_addr(&my_addr);

	// Initial connection configuration
	mesh_open(&zoundtracker_conn, CHANNEL, &zoundtracker_callbacks);
	
	packetbuf_copyfrom("hello", 5);
	resends = 0;			
	sink_addr.u8[0] = SINK_ADDR1;
	sink_addr.u8[1] = SINK_ADDR2;
	mesh_send(&zoundtracker_conn, &sink_addr);
		
	etimer_set(&timer, TIME);	
	nfile = 0;		
		
	while (1)
	{
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));
		printf("timer_expired\n");
		
		// Writing data into a current file
		sprintf(filename, "file%d", nfile);
		fd = cfs_open(filename, CFS_APPEND);
		cfs_write(fd,"b",1);
		cfs_close(fd);

		etimer_reset(&timer);
	}

	PROCESS_END();
}
