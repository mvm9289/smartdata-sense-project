#include "contiki.h"
#include "leds.h"
#include "cfs/cfs.h"
#include "net/rime.h"
#include "net/rime/mesh.h"
#include "adxl345.h"
#include <stdio.h>
#include <string.h>
#include "zt-packet-mgmt.h"

//------------------------------------------------------------------------------

// CFS Defines
#define FILE_NAME_SIZE 8
#define READ_BUFFER_SIZE 80
#define ACCEL_BUFFER_SIZE 5
#define MAX_WRITES 2
#define MAX_FILES 50
#define MAX_ATTEMPTS 3
#define NUM_SECONDS_WRITE 5

// NET Defines
#define MY_ADDR1 1  // (!) Update for every mn
#define MY_ADDR2 0
//#define NUM_SECONDS_HELLO 120

//------------------------------------------------------------------------------

// CFS variables
static int write_file, send_file, write_num, write_bytes, read_bytes, fd, i, message_type;
static struct etimer write_timer;
static unsigned char filename[FILE_NAME_SIZE];
static unsigned char read_buffer[READ_BUFFER_SIZE];
    
// NET variables
static int attempts;
static struct ctimer hello_timer;
static rimeaddr_t sink_addr;
static struct mesh_conn zoundtracker_conn;

// Sensor variables
static int x, accel_bytes;
static unsigned char accel_buffer[ACCEL_BUFFER_SIZE];

//------------------------------------------------------------------------------

// NET Functions
static void hello_msg() 
{   
    // 0. Configure MN state
    message_type = HELLO_MN;
 
    // 1. Construction of "packet"
    Packet my_packet;
    my_packet.addr1 = MY_ADDR1;
    my_packet.addr2 = MY_ADDR2;
    my_packet.type = HELLO_MN;
    my_packet.size = HELLO_MSG_SIZE;    
    my_packet.counter = 0;
    my_packet.data = {'h', 'e', 'l', 'l', 'o'};
    my_packet.checksum = compute_checksum(&my_packet);
    
    unsigned char[PACKET_SIZE] my_array = mount_packet(&my_packet);
    packetbuf_copyfrom((void *)my_array, PACKET_SIZE);
	
	sink_addr.u8[0] = SINK_ADDR1;
	sink_addr.u8[1] = SINK_ADDR2;
	mesh_send(&zoundtracker_conn, &sink_addr);
	
	// 2. Debug info
	printf("[net] sending 'hello' message\n\n");
}

static void data_msg(unsigned char *data) 
{   
    // 0. Configure MN state
    message_type = DATA;
 
    // 1. Construction of "packet"
    Packet my_packet;
    my_packet.addr1 = MY_ADDR1;
    my_packet.addr2 = MY_ADDR2;
    my_packet.type = DATA;
    my_packet.size = (sizeof)data;    
    my_packet.counter = 0;
    my_packet.data = {'h', 'e', 'l', 'l', 'o'}; // File fragmentation (!)
    my_packet.checksum = compute_checksum(&my_packet);
    
    unsigned char[PACKET_SIZE] my_array = mount_packet(&my_packet);
    packetbuf_copyfrom((void *)my_array, PACKET_SIZE);
	
	sink_addr.u8[0] = SINK_ADDR1;
	sink_addr.u8[1] = SINK_ADDR2;
	mesh_send(&zoundtracker_conn, &sink_addr);
	
	// 2. Debug info
	printf("[net] sending 'hello' message\n\n");
}


static void sent(struct mesh_conn *c) 
{
    // 0. Debug info
    if (message_type == HELLO_MN)
    {   
        printf("[net] sent 'HELLO_MN' message\n\n");
    }
    else
    {
        printf("[net] sent 'DATA' message\n\n");
    }  
}

static void timedout(struct mesh_conn *c) 
{
    // Resending the current send file
    attempts++;
	if (attempts < MAX_ATTEMPTS) 
	{
	    if (message_type == HELLO_MN)
	    {
	         // 1. Building and sending "HELLO_MN"
	        hello_msg();
	        
	        // 2. Debug info
	        printf("[net] timeout resending 'hello' message\n\n");
	    }
	    else
	    {
	        printf("[net] timeout resending the current file\n filename: file%d\n\n", send_file);
	        packetbuf_copyfrom((void*)read_buffer, read_bytes);
	    }
	    
		sink_addr.u8[0] = SINK_ADDR1;
		sink_addr.u8[1] = SINK_ADDR2;
		mesh_send(&zoundtracker_conn, &sink_addr);
	}
	else
	{
	    printf("[net] maximum number of attempts reached\n message lost\n\n");
        // 'Error' mode started
        leds_on(LEDS_RED);
	}
}

static void received(struct mesh_conn *c, const rimeaddr_t *from, uint8_t hops) 
{
	if (!memcmp(packetbuf_dataptr(), (void *)"poll", 4)) 
	{    
		printf("[net] 'poll' message received\n\n");
		// 'Hello' mode finished
		leds_off(LEDS_GREEN);
		// 'Poll' mode started
		leds_on(LEDS_YELLOW);
		
		
		if (send_file >= write_file)
		{
		    printf("[net] all pending files are sended\n no response\n\n");
		}
		else
		{
	        // Generating the filename of the current sending file
	        sprintf(filename, "file%d", send_file);
	    
	        // Reading the current sending file
	        fd = cfs_open(filename, CFS_READ);
	        if (fd < 0)
	        {
	            printf("[cfs] error openning file\n filename: %s\n\n", filename);
	        }
	        else 
	        {   
	            //printf("[cfs] open: file open successful (%s)\n", filename);        
                
                read_bytes = cfs_read(fd, read_buffer, READ_BUFFER_SIZE);
                if (read_bytes < 1)
	            {
	                printf("[cfs] error reading from the file\n num. bytes readed: %d\n\n", write_bytes);
	            }
	            else
	            {
	                //printf("[cfs] read: file read successful (%d)(%s)\n", read_bytes, read_buffer);
	            
	                printf("[net] sending the current file\n filename: file%d\n\n", send_file);
	                
	                // Sending the current sending file
	                packetbuf_copyfrom((void*)read_buffer, read_bytes);
	                attempts = 0;			
			        sink_addr.u8[0] = SINK_ADDR1;
			        sink_addr.u8[1] = SINK_ADDR2;
			        mesh_send(&zoundtracker_conn, &sink_addr);     
                }
                
                cfs_close(fd);
	        } 
	    }
	}
}

const static struct mesh_callbacks zoundtracker_callbacks = {received, sent, timedout};

//------------------------------------------------------------------------------

