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
#define ACCEL_BUFFER_SIZE 5
#define MAX_ATTEMPTS 5
#define NUM_SECONDS_WRITE 60
#define WORKING_FILE 1
#define ERROR -1
#define NO_NEXT_PACKET -1

// NET Defines
#define MY_ADDR1 1  // (!) Update for every mn
#define MY_ADDR2 0
//#define NUM_SECONDS_HELLO 120

//------------------------------------------------------------------------------

// CFS variables
static int write_file, send_file, write_num, write_bytes, read_bytes, fd, i, message_type;
static struct etimer write_timer;
static unsigned char read_buffer[DATA_SIZE], packet_number;
    
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
    // This function builds and sends a "HELLO_MN" message to de "Basestation"    

    // 0. Configure MN state
    message_type = HELLO_MN;
 
    // 1. "Packet" construction
    Packet my_packet;
    my_packet.addr1 = MY_ADDR1;
    my_packet.addr2 = MY_ADDR2;
    my_packet.type = HELLO_MN;
    my_packet.size = HELLO_MSG_SIZE;    
    my_packet.counter = 0;
    my_packet.data = {'h', 'e', 'l', 'l', 'o'};
    my_packet.checksum = compute_checksum(&my_packet);
    
    // 2. "Packet" transform
    unsigned char[PACKET_SIZE] my_array = mount_packet(&my_packet);
    packetbuf_copyfrom((void *)my_array, PACKET_SIZE);
	
	// 3. "Packet" send
	sink_addr.u8[0] = SINK_ADDR1;
	sink_addr.u8[1] = SINK_ADDR2;
	mesh_send(&zoundtracker_conn, &sink_addr);
	
	printf("[net] sending 'HELLO_MN' message\n\n");
}

static void data_msg() 
{   
    // This function builds and sends a "DATA" message to the "Basestation"

    // 0. Configure MN state
    message_type = DATA;
 
    // 1. "Packet" construction
    Packet my_packet;
    my_packet.addr1 = MY_ADDR1;
    my_packet.addr2 = MY_ADDR2;
    my_packet.type = DATA;
    my_packet.size = (sizeof)read_buffer;    
    my_packet.counter = packet_number*DATA_SIZE; // File offset
    my_packet.data = read_buffer; // File fragmentation (!)
    my_packet.checksum = compute_checksum(&my_packet);
    
    // 2. "Packet" transform
    unsigned char[PACKET_SIZE] my_array = mount_packet(&my_packet);
    packetbuf_copyfrom((void *)my_array, PACKET_SIZE);
	
	// 3. "Packet" send
	sink_addr.u8[0] = SINK_ADDR1;
	sink_addr.u8[1] = SINK_ADDR2;
	mesh_send(&zoundtracker_conn, &sink_addr);
	
	printf("[net] sending 'DATA' message\n\n");
}

static void send_packet_from_file(void)
{
    // This function sends the next packet identified by the 'packet_number' 
    // from the "WORKING_FILE" to the "Basestation".

    // 0. Reading from the "WORKING_FILE"
    fd = cfs_open(WORKING_FILE, CFS_READ);
	if (fd == ERROR)
	  printf("[cfs] error openning the 'WORKING_FILE'\n\n");
	else 
	{                  
        read_bytes = cfs_read(fd, read_buffer, DATA_SIZE);
        if (read_bytes == ERROR)
          printf("[cfs] error reading from the 'WORKING_FILE'\n\n");
        else if (read_bytes == 0)
          packet_number = NO_NEXT_PACKET;
        else
        {  
            // There's information to send
            
            // 1. "Packet" sending
            data_msg();
            
            printf("[net] sending the 'WORKING_FILE' (packet number: %d)\n\n", packet_number);			
        }
        cfs_close(fd);
    }
}

static void ack_received(void)
{
    // This function sends the next packet of the "WORKING_FILE" to the 
    // "Basestation" till the end of the file. When the end of the file is 
    // reached removes the "WORKING_FILE" and reopens it.
    
    if (message_type == HELLO_ACK)
      printf("[net] 'HELLO_ACK' received\n\n");
    else if (message_type == DATA_ACK)
    {
      printf("[net] 'DATA_ACK' received\n\n");
      
      if (packet_number != NO_NEXT_PACKET)
      {        
        // 0. Sending the next packet from the "WORKING_FILE"
        packet_number++;
        send_packet_from_file();
      }
      else 
      {
        // There's no more packets to send
        
        // 1. 'WORKING_FILE' completely sended, removing it
        cfs_remove(WORKING_FILE);
      }
    }
}

//------------------------------------------------------------------------------
static void sent(struct mesh_conn *c) 
{
    // Checksum comprobation needed on receiver
    if (message_type == HELLO_MN)
      printf("[net] sent 'HELLO_MN' message\n\n");
    else if (message_type == DATA)
      printf("[net] sent 'DATA' message\n\n"); 
    
    leds_on(LEDS_GREEN);
}

//------------------------------------------------------------------------------
static void timedout(struct mesh_conn *c) 
{
    attempts++;
	if (attempts < MAX_ATTEMPTS) 
	{
	    if (message_type == HELLO_MN)
	    {
	         // 1. Resending "HELLO_MN" message
	        hello_msg();
	        
	        printf("[net] timeout resending 'HELLO_MN' message\n\n");
	    }
	    else if (message_type == DATA)
	    {
	        // 2. Resending "DATA" message
	        send_packet_from_file();
        
            printf("[net] timeout resending the current packet"); 
	        printf("(packet number: %d) from the 'WORKING_FILE'\n\n", packet_number);
        }
	}
	else
	{
	    printf("[net] maximum number of attempts reached\n");
	    if (message_type == HELLO_MN)
	      printf(" 'HELLO_MN' message lost\n\n");
	    else if (message_type == DATA)
	      printf(" 'DATA' message lost (packet number: %d)\n\n", packet_number);
        
        leds_on(LEDS_RED);
	}
}

// (!) Funcionalidades anteriores editadas (primera revision). Siguientes 
// pendientes de adaptar 

//------------------------------------------------------------------------------
static void received(struct mesh_conn *c, const rimeaddr_t *from, uint8_t hops) 
{
    // This function sends a "HELLO_MN" if "HELLO_BS" is received or sends the
    // "WORKING_FILE" if the "Basestation" requests data. 
    
    attempts = 0; 
    
    // 0. Obtaining the "Packet"
    Packet my_packet = mount(packetbuf_dataptr());
    
    // 1. Response depending on the "type" value
    if (my_packet.type == HELLO_BS)
    {
        // 2. Sending "HELLO_MN" message
        hello_msg()
    }
    else if (my_packet.type == POLL)
    {
        // 3. Sending "DATA" messages from "WORKING_FILE"
        packet_number = 0;
        send_packet_from_file()
    }
    else if (my_packet.type == ACK)
    {
        // 4. Sending the next packet or erasing data from "WORKING_FILE"
        ack_received();
    }
    
    leds_off(LEDS_GREEN)
    leds_off(LEDS_RED)
}

const static struct mesh_callbacks zoundtracker_callbacks = {received, sent, timedout};

//------------------------------------------------------------------------------

