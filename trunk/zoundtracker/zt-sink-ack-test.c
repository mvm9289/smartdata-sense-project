#include <stdio.h>
#include <string.h>
#include "contiki.h"
#include "leds.h"
#include "net/rime.h"
#include "net/rime/mesh.h"
#include "lib/zt-packet-mgmt.h"

//------------------------------------------------------------------------------

// NET Defines
#define MY_ADDR1 1  // (!) Update for every mn
#define MY_ADDR2 0
#define MAX_RESENDS 5

//------------------------------------------------------------------------------

// NET Variables
static rimeaddr_t mn_addr;
static struct mesh_conn zoundtracker_conn;
unsigned int resends;

//------------------------------------------------------------------------------

// NET functions 

static void ack_message(void)                                                                                                          
{
    // This function builds and sends a "HELLO_ACK" message to de "Mobile Node"
    
    // 1. "Packet" construction
    Packet my_packet;
    my_packet.addr1 = SINK_ADDR1;
    my_packet.addr2 = SINK_ADDR2;
    my_packet.type = HELLO_ACK;
    my_packet.size = 0;    
    my_packet.counter = 0;
    my_packet.checksum = compute_checksum(&my_packet);                                                                             
    
    // 2. "Packet" transform
    unsigned char my_array[PACKET_SIZE];
    mount_packet(&my_packet, my_array);
    packetbuf_copyfrom((void *)my_array, PACKET_SIZE);
    
	// 3. "Packet" send
	mn_addr.u8[0] = MY_ADDR1;
	mn_addr.u8[1] = MY_ADDR2;
	mesh_send(&zoundtracker_conn, &mn_addr);
	
	printf("[net] sending 'HELLO_ACK' message\n\n");                                                                                                                          
}                                                                                                                              

//------------------------------------------------------------------------------
static void sent(struct mesh_conn *c)
{
    printf("Packet sent.\n\n");
    
    leds_on(LEDS_GREEN);
}

//------------------------------------------------------------------------------
static void timedout(struct mesh_conn *c)
{
    printf("Packet timeouted. Resends = %d\n\n", resends+1);
    
    resends++;
    if (resends < MAX_RESENDS)
      ack_message();
    else
      leds_on(LEDS_RED);
}

//------------------------------------------------------------------------------
static void received(struct mesh_conn *c, const rimeaddr_t *from, uint8_t hops)
{  
    // 0. Sending new ack message
    resends = 0;
    ack_message();
    
    leds_off(LEDS_GREEN);
    leds_off(LEDS_RED);
}

const static struct mesh_callbacks zoundtracker_sink_callbacks = {received, sent, timedout};
//------------------------------------------------------------------------------

// Process
PROCESS(example_zoundt_sink_process, "Example zoundt sink process");
AUTOSTART_PROCESSES(&example_zoundt_sink_process);

PROCESS_THREAD(example_zoundt_sink_process, ev, data) {   
    
    PROCESS_EXITHANDLER(mesh_close(&zoundtracker_conn);)
    PROCESS_BEGIN();
    
    // NET Initialization                                                                                                  
    rimeaddr_t my_addr;                                                                                               
    my_addr.u8[0] = SINK_ADDR1;                                                                                       
    my_addr.u8[1] = SINK_ADDR2;                                                                                       
    rimeaddr_set_node_addr(&my_addr);                                                                                 
    mesh_open(&zoundtracker_conn, CHANNEL, &zoundtracker_sink_callbacks);                                             
      
    while (1)
    {
        PROCESS_WAIT_EVENT();
        
        printf("[event] unknown event\n\n");
    }
    
    PROCESS_END();
}
