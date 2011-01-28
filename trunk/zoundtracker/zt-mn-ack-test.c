#include "contiki.h"
#include "leds.h"
#include "cfs/cfs.h"
#include "net/rime.h"
#include "net/rime/mesh.h"
#include "net/rime/trickle.h"
#include "adxl345.h"
#include <stdio.h>
#include <string.h>
#include "lib/zt-packet-mgmt.h"

//------------------------------------------------------------------------------

// CFS Defines
#define MAX_ATTEMPTS 5
#define NUM_SECONDS_SAMPLE 60
#define WORKING_FILE "my_file"
#define ERROR -1
#define NO_NEXT_PACKET -1

// NET Defines
#define MY_ADDR1 1  // (!) Update for every mn
#define MY_ADDR2 0
#define EMPTY 0

// Sensor Defines
#define SAMPLE_SIZE 1

// State Defines
#define BLOCKED 1
#define DATA_COLLECT 2
#define DATA_SEND 3

//------------------------------------------------------------------------------

// CFS variables
static int write_bytes, read_bytes, fd, sample_number, packet_number;
static unsigned short file_size;
static struct etimer control_timer;
static unsigned char read_buffer[DATA_SIZE], input_msg_type, output_msg_type;
    
// NET variables
static int attempts;
static rimeaddr_t sink_addr;
static struct mesh_conn zoundtracker_conn;
static struct trickle_conn zoundtracker_broadcast_conn;

// Sensor variables
static char sensor_sample;

// State variables
static unsigned char state;

//------------------------------------------------------------------------------

// NET Functions
static void hello_msg() 
{
    // This function builds and sends a "HELLO_MN" message to de "Basestation"    

    // 0. Configure MN state
    output_msg_type = HELLO_MN;
 
    // 1. "Packet" construction
    Packet my_packet;
    my_packet.addr1 = MY_ADDR1;
    my_packet.addr2 = MY_ADDR2;
    my_packet.type = HELLO_MN;
    my_packet.size = HELLO_MSG_SIZE;    
    my_packet.counter = 0;
    my_packet.checksum = compute_checksum(&my_packet);
    
    // 2. "Packet" transform
    unsigned char my_array[PACKET_SIZE];
    mount_packet(&my_packet, my_array);
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
    output_msg_type = DATA;
 
    // 1. "Packet" construction
    Packet my_packet;
    my_packet.addr1 = MY_ADDR1;
    my_packet.addr2 = MY_ADDR2;
    my_packet.type = DATA;
    my_packet.size = file_size;    
    my_packet.counter = (packet_number-1)*DATA_SIZE + my_packet.size; // File offset
    memcpy(my_packet.data, read_buffer, read_bytes); // File fragmentation (!)
    my_packet.checksum = compute_checksum(&my_packet);
    
    // 2. "Packet" transform
    unsigned char my_array[PACKET_SIZE];
    mount_packet(&my_packet, my_array);
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

static void ack_received(unsigned char type)
{
    // This function sends the next packet of the "WORKING_FILE" to the 
    // "Basestation" till the end of the file. When the end of the file is 
    // reached removes the "WORKING_FILE" and reopens it.
    
    if (type == HELLO_ACK)
    {
        printf("[net] 'HELLO_ACK' received\n\n");
        output_msg_type = EMPTY;
    }
    else if (type == DATA_ACK)
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
        output_msg_type = EMPTY;
        
        // 1. 'WORKING_FILE' completely sended, removing it
        cfs_remove(WORKING_FILE);
        sample_number = 0;
        file_size = 0;
      }
    }
}

//------------------------------------------------------------------------------
static void sent(struct mesh_conn *c) 
{
    // Checksum comprobation needed on receiver
    if (output_msg_type == HELLO_MN)
      printf("[net] sent 'HELLO_MN' message\n\n");
    else if (output_msg_type == DATA)
      printf("[net] sent 'DATA' message\n\n"); 
    
    leds_on(LEDS_GREEN);
}

//------------------------------------------------------------------------------
static void timedout(struct mesh_conn *c) 
{
    attempts++;
	if (attempts < MAX_ATTEMPTS) 
	{
	    if (output_msg_type == HELLO_MN)
	    {
	         // 1. Resending "HELLO_MN" message
	        hello_msg();
	        
	        printf("[net] timeout resending 'HELLO_MN' message\n\n");
	    }
	    else if (output_msg_type == DATA)
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
	    if (output_msg_type == HELLO_MN)
	      printf(" 'HELLO_MN' message lost\n\n");
	    else if (output_msg_type == DATA)
	      printf(" 'DATA' message lost (packet number: %d)\n\n", packet_number);
        
        // Starting new sample period (10 minutes)
        sample_number = 0;
        
        leds_on(LEDS_RED);
	}
}

// (!) Funcionalidades anteriores editadas (primera revision). Siguientes 
// pendientes de adaptar 

//------------------------------------------------------------------------------
static void received(struct mesh_conn *c, const rimeaddr_t *from, uint8_t hops) 
{
    // This function sends a "HELLO_MN" if "HELLO_BS" is received or sends the
    // "WORKING_FILE" if the "Basestation" requests data. If there's a message
    // already sending, this message is saved. If there's another message
    // saved, the last message is discarded.
    
    // 0. Obtaining the "Packet"
    Packet my_packet;
    my_packet = unmount_packet(packetbuf_dataptr());
    if (compute_checksum(&my_packet) == my_packet.checksum)
    {
        // Valid message
            leds_on(LEDS_YELLOW);
    
        if (my_packet.type == HELLO_ACK || my_packet.type == DATA_ACK)
        {
            // 1. Sending the next packet or erasing data from "WORKING_FILE"
            ack_received(my_packet.type);
        
            leds_off(LEDS_GREEN);
            leds_off(LEDS_RED);
        }
        
        if ((input_msg_type != EMPTY && output_msg_type == EMPTY) || (my_packet.type != HELLO_ACK && my_packet.type != DATA_ACK && output_msg_type == EMPTY))
        {
            // There's a message saved ready to reply or the message received is not
            // an ACK and we can reply it.
            
            attempts = 0;
            
            if (input_msg_type == EMPTY)
              input_msg_type = my_packet.type;
          
            // 2. Response depending on the "type" value
            if (input_msg_type == HELLO_BS)
            {
                // 3. Sending "HELLO_MN" message
                hello_msg();
            }
            else if (input_msg_type == POLL)
            {
                // 4. Sending "DATA" messages from "WORKING_FILE"
                packet_number = 1;
                send_packet_from_file();
            }
            
            input_msg_type = EMPTY;
            
            leds_off(LEDS_GREEN);
            leds_off(LEDS_RED);
        }
        else if (input_msg_type == EMPTY && output_msg_type != EMPTY)
        {
            // There's a message already sending. The input message is saved
            input_msg_type = my_packet.type;
        }
        // else the message is discarded
    }
    // else invalid message
}

static void broadcast_received(struct trickle_conn* c)
{
    // "HELLO_BS" message received, replying trough mesh connection
    //received();
}

const static struct mesh_callbacks zoundtracker_callbacks = {received, sent, timedout};
const static struct trickle_callbacks zoundtracker_broadcast_callbacks = {broadcast_received};

//------------------------------------------------------------------------------

// Sensor functions
void get_sensor_sample(void)
{
    // This functions reads data of the x axis from the accelerometer	
	
	// 0. Reading data from sensor
	sensor_sample = (char)accm_read_axis(X_AXIS);
	
	printf("[accel] x axis readed\n value: %d\n\n", sensor_sample);

    // 1. Writing data into the "WORKING_FILE"
	fd = cfs_open(WORKING_FILE, CFS_WRITE | CFS_APPEND);       
    if (fd == ERROR) 
      printf("[cfs] error openning the 'WORKING_FILE'\n\n");
    else 
    {
        write_bytes = cfs_write(fd, &sensor_sample, SAMPLE_SIZE);
        if (write_bytes != SAMPLE_SIZE) 
          printf("[cfs] write: error writing into the 'WORKING_FILE'\n\n");
        else
          file_size += write_bytes;
                  
        cfs_close(fd);
        
    }
}
//------------------------------------------------------------------------------

// Process
PROCESS(example_zoundt_mote_process, "Example zoundt mote process");
AUTOSTART_PROCESSES(&example_zoundt_mote_process);

PROCESS_THREAD(example_zoundt_mote_process, ev, data) {   
    
    PROCESS_EXITHANDLER(mesh_close(&zoundtracker_conn);)
    PROCESS_BEGIN();
    
    // NET Initialization
    rimeaddr_t my_addr;
	my_addr.u8[0] = MY_ADDR1;
	my_addr.u8[1] = MY_ADDR2;
	rimeaddr_set_node_addr(&my_addr);
	mesh_open(&zoundtracker_conn, CHANNEL, &zoundtracker_callbacks);                                             
	       
    // CFS Initialization
    //etimer_set(&control_timer, NUM_SECONDS_SAMPLE*CLOCK_SECOND);
    sample_number = 0;
    file_size = 0;
    
    // Sensor Initialization
    accm_init();

    // State Initialization
    state = BLOCKED;
    
    // 0. Send first "HELLO_MN"
    hello_msg();
    
    printf("[state] current state 'BLOCKED'\n\n");
    
    while (1)
    {
        PROCESS_WAIT_EVENT();
        /*if (ev == PROCESS_EVENT_TIMER)
        {
            printf("[event] timer expired event\n\n");
            
            if (state == BLOCKED)
            {
                // 1. Changing to "Data Collect" state
                state = DATA_COLLECT;
                printf("[state] current state 'Data Collect'\n\n");
            
                get_sensor_sample();
                printf("[sensor] sample measured (sample number: %d)\n\n", sample_number); // 'sample_number' starts on zero
            
                // 2. Updating state
                sample_number++;
            
                if (sample_number == 10)
                {    
                    // 3. Changing to "Data Send" state
                    state = DATA_SEND;
                    printf("[state] current state 'Data Send'\n\n");
                    
                    send_packet_from_file();
                    
                    sample_number = 0;                
                }
            }       
        }
        else
          printf("[event] unknown event\n\n");  
    
        // 4. Reseting the timer 
        etimer_set(&control_timer, NUM_SECONDS_SAMPLE*CLOCK_SECOND);*/
    }
        
    PROCESS_END(); 
}
