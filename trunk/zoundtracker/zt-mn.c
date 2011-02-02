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

#ifdef TRUE
#undef TRUE
#endif

#define TRUE 1

#ifdef FALSE
#undef FALSE
#endif

#define FALSE 0

// CFS Defines
#define MAX_ATTEMPTS 5
#define NUM_SECONDS_SAMPLE 6
#define WORKING_FILE "my_file"
#define ERROR -1
#define NO_NEXT_PACKET -1

// NET Defines
//#define MY_ADDR1 1  // (!) Update for every mn
//#define MY_ADDR2 0
#define EMPTY 99

// Sensor Defines
#define SAMPLE_SIZE 1

// State Defines
#define BLOCKED 1
#define DATA_COLLECT 2
#define DATA_SEND 3

//------------------------------------------------------------------------------

// CFS variables
static int write_bytes, read_bytes, fd_read, fd_write, sample_number, packet_number, ack_timeout;
static unsigned short file_size;
static struct etimer control_timer;
static unsigned char read_buffer[DATA_SIZE], input_msg_type, output_msg_type;
    
// NET variables
static int attempts;
static rimeaddr_t sink_addr;
static struct mesh_conn zoundtracker_conn;
static struct trickle_conn zoundtracker_broadcast_conn;
static unsigned char my_array[PACKET_SIZE], next_packet;
static unsigned short packet_checksum;

// Sensor variables
static char sensor_sample;

// State variables
static unsigned char state;

//------------------------------------------------------------------------------

// NET Functions
static void hello_msg() 
{
    // This function builds and sends a "HELLO_MN" message to de "Basestation"    
    Packet my_packet_send;

    // 0. Configure MN state
    output_msg_type = HELLO_MN;
 
    // 1. "Packet" construction
    my_packet_send.addr1 = MY_ADDR1;
    my_packet_send.addr2 = MY_ADDR2;
    my_packet_send.type = HELLO_MN;
    my_packet_send.size = HELLO_MSG_SIZE;    
    my_packet_send.counter = 0;
    my_packet_send.checksum = compute_checksum(&my_packet_send);
    
    // 2. "Packet" transform
    mount_packet(&my_packet_send, my_array);
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
    Packet my_packet_send;
    
    // 0. Configure MN state
    output_msg_type = DATA;
 
    // 1. "Packet" construction
    my_packet_send.addr1 = MY_ADDR1;
    my_packet_send.addr2 = MY_ADDR2;
    my_packet_send.type = DATA;
    my_packet_send.size = file_size;    
    my_packet_send.counter = (packet_number-1)*DATA_SIZE; // File offset
    memcpy(my_packet_send.data, read_buffer, read_bytes); // File fragmentation (!)
    my_packet_send.checksum = compute_checksum(&my_packet_send);
    
    printf("[net]----------- DATA' packet contents:---------\n");
    printf("Addr1: %d\n", my_packet_send.addr1);
    printf("Addr2: %d\n", my_packet_send.addr2);
    printf("Type: %d\n", my_packet_send.type);
    printf("Size: %d\n", my_packet_send.size);
    printf("Counter: %d\n", my_packet_send.counter);
    printf("Data: %c\n", my_packet_send.data[0]);
    printf("Checksum: %d\n", my_packet_send.checksum);
    printf("---------------------------------------------------\n\n");
    
    // 2. "Packet" transform
    mount_packet(&my_packet_send, my_array);
    packetbuf_copyfrom((void *)my_array, PACKET_SIZE);
	
	// 3. "Packet" send
	sink_addr.u8[0] = SINK_ADDR1;
	sink_addr.u8[1] = SINK_ADDR2;
	mesh_send(&zoundtracker_conn, &sink_addr);
	
	printf("[net] sending 'DATA' message\n\n");
}

static unsigned char prepare_packet(void)
{
    // This function sends the next packet identified by the 'packet_number' 
    // from the "WORKING_FILE" to the "Basestation".

    // 0. Reading from the "WORKING_FILE"
    if (fd_read == EMPTY)
    {
        // First packet of "WORKING_FILE"
        packet_number = 0;
        fd_read = cfs_open(WORKING_FILE, CFS_READ);
	}
	// else currently sending 'WORKING_FILE'
	
	if (fd_read == ERROR)
	{  
	    printf("[cfs] error openning the 'WORKING_FILE'\n\n");
	    return FALSE;
	}
	else 
	{                  
        read_bytes = cfs_read(fd_read, read_buffer, DATA_SIZE);
        printf("\n------BYTES READED: %d--------\n\n", read_bytes);
        
        if (read_bytes == ERROR)
        {  
            printf("[cfs] error reading from the 'WORKING_FILE'\n\n");
            return FALSE;  
        }
        else if (read_bytes == 0) {
          return FALSE;
        }
        // else There's information to send
        
    }
    
    return TRUE;
}

static void send_packet_from_file(void)
{
    next_packet = prepare_packet();
          
    if (next_packet)
    {              
        // 1. "Packet" sending
        packet_number++;
        data_msg();
        printf("[net] sending the 'WORKING_FILE' (packet number: %d)\n\n", packet_number);
    }    
    else  
    {
        // There's no more packets to send
        output_msg_type = EMPTY;
        
        // 1. 'WORKING_FILE' completely sended, removing it
        cfs_close(fd_read);
        fd_read = EMPTY;
        cfs_remove(WORKING_FILE);
        sample_number = 0;
        file_size = 0;
        
        // (!) "WORKING_FILE" sended to the "Sink".
        // Changing to "BLOCKED" from "DATA_SEND" state  
        state = BLOCKED;
        printf("[state] current state 'BLOCKED'\n\n");
    }
}

static void ack_received(unsigned char type)
{
    // This function sends the next packet of the "WORKING_FILE" to the 
    // "Basestation" till the end of the file. When the end of the file is 
    // reached removes the "WORKING_FILE" and reopens it.
    
    if (output_msg_type != EMPTY)
    {
        // ACK message that we're waiting for
        ack_timeout = 0;
        
        if (type == HELLO_ACK)
        {
            printf("[net] 'HELLO_ACK' received\n\n");
            output_msg_type = EMPTY;
            
            // (!) Discover/handshake finished.
            // Changing to "BLOCKED" from "DATA_SEND" state  
            state = BLOCKED;
            printf("[state] current state 'BLOCKED'\n\n");
        }
        else if (type == DATA_ACK)
        {
          printf("[net] 'DATA_ACK' received\n\n");
          
          // If it's necessary sends next packet           
          send_packet_from_file();
        }
    }
    // else ACK message is discarded
}

static void file_send_failed(void)
{
    if (output_msg_type == DATA)
    {       
        cfs_close(fd_read);
        fd_read = EMPTY;
        sample_number = 0;
        printf("[net] DATA_ACK message lost\n\n");
    }
    else
    {
        printf("[net] HELLO_ACK message lost\n\n");
    }
        
    // Current sending message lost. We're not pending of "DATA_ACK".    
    output_msg_type = EMPTY;
    ack_timeout = 0;
}                    

//------------------------------------------------------------------------------
static void sent(struct mesh_conn *c) 
{
    attempts = 0;
    
    // Checksum comprobation needed on receiver
    if (output_msg_type == HELLO_MN)
      printf("[net] sent 'HELLO_MN' message\n\n");
    else if (output_msg_type == DATA)
      printf("[net] sent 'DATA' message\n\n"); 
    
    // Waiting for an ACK message
    ack_timeout = 1;
    
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
	        data_msg();
        
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
        
        // Current sending message lost. We can't erase the "WORKING_FILE"
        file_send_failed();                
        
        // (!) Message lost.
        // Changing to "BLOCKED" from "DATA_SEND" state  
        state = BLOCKED;
        printf("[state] current state 'BLOCKED'\n\n");
        
        // Starting new sample period (10 minutes)
        sample_number = 0;
        
        leds_on(LEDS_RED);
	}
}

//------------------------------------------------------------------------------
static void received(struct mesh_conn *c, const rimeaddr_t *from, uint8_t hops) 
{
    // This function sends the "WORKING_FILE" if the "Basestation" requests 
    // data. If there's a message already sending, this message is saved. If 
    // there's another message saved, the last message is discarded.
       
    // 0. Obtaining the "Packet" and checking checksum
    Packet my_packet;
    my_packet = unmount_packet(packetbuf_dataptr());
    packet_checksum = compute_checksum(&my_packet);
    
    if (packet_checksum == my_packet.checksum)
    {
        // (!) Message received ("POLL/HELLO_ACK/DATA_ACK").
        // Changing to "DATA_SEND" from "DATA_SEND/BLOCKED/DATA_COLLECT" state  
        state = DATA_SEND;
        printf("[state] current state 'DATA_SEND'\n\n");

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
            
            if (input_msg_type == EMPTY)
            {
                input_msg_type = my_packet.type;
            }
            
            // 2. Response depending on the "type" value
            if (input_msg_type == HELLO_BS)
            {
                printf("[net] 'HELLO_BS' message received\n\n");
                
                // 3. Sending "HELLO_MN" message
                hello_msg();
            }
            else if (input_msg_type == POLL)
            {
                printf("[net] 'POLL' message received\n\n");
                if (output_msg_type == DATA)
                {
                    // 4."POLL" message reply to a "DATA" message sended. 
                    // Resending the last packet.
                    data_msg(); 
                }
                else 
                {
                    // 5. Sending next "DATA" messages from "WORKING_FILE"
                    send_packet_from_file();
                }
            }
            
            input_msg_type = EMPTY;
            
            leds_off(LEDS_GREEN);
            leds_off(LEDS_RED);
        }
        else if (input_msg_type == EMPTY && output_msg_type != EMPTY && my_packet.type != HELLO_ACK && my_packet.type != DATA_ACK)
        {
            // There's a message already sending. The input message is saved
            input_msg_type = my_packet.type;
        }
        else 
        {
            // Input is empty or message is discarded 
            printf("[net] 'input_msg_type == EMPTY' or message is discarded\n\n");
        }
    }
    else 
    {
        // invalid message
        printf("[net] incorrect checksum invalid message\n\n");
    }
}

static void broadcast_received(struct trickle_conn* c)
{
    // This function sends a "HELLO_MN" message through the "mesh" connection when
    // a "broadcast" "HELLO_BS" message received. The behaviour is very similar 
    // to "received" mesh callback to attend only the "HELLO_BS" messages sended 
    // through the "broadcast" connection.
    
    // 0. Obtaining the "Packet" and checking checksum
    Packet my_packet;
    my_packet = unmount_packet(packetbuf_dataptr());
    packet_checksum = compute_checksum(&my_packet);
    
    if (packet_checksum == my_packet.checksum)
    {
        // (!) Message received ("HELLO_BS").
        // Changing to "DATA_SEND" from "DATA_SEND/BLOCKED/DATA_COLLECT" state  
        state = DATA_SEND;
        printf("[state] current state 'DATA_SEND'\n\n");

        // Valid message
        leds_on(LEDS_YELLOW);
        
        if (output_msg_type == EMPTY)
        {
            // There's a message saved ready to reply or the message received is not
            // an ACK and we can reply it.
            
            attempts = 0;
            
            if (input_msg_type == EMPTY)
              input_msg_type = my_packet.type;
          
            // 2. Response depending on the "type" value
            if (input_msg_type == HELLO_BS)
            {
                printf("[net] 'HELLO_BS' message received\n\n");
                
                // 3. Sending "HELLO_MN" message
                hello_msg();
            }
            
            input_msg_type = EMPTY;
            
            leds_off(LEDS_GREEN);
            leds_off(LEDS_RED);
        }
        else if (input_msg_type == EMPTY && output_msg_type != EMPTY && my_packet.type != HELLO_ACK && my_packet.type != DATA_ACK)
        {
            // There's a message already sending. The input message is saved
            input_msg_type = my_packet.type;
        }
        else 
        {
            //the message is discarded
            printf("[net] 'input_msg_type != EMPTY' message is discarded\n\n");
        }
    }
    else 
    {
        // invalid message
        printf("[net] incorrect checksum invalid message\n\n");
    }
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
	fd_write = cfs_open(WORKING_FILE, CFS_WRITE | CFS_APPEND);       
    if (fd_write == ERROR) 
      printf("[cfs] error openning the 'WORKING_FILE'\n\n");
    else 
    {
        write_bytes = cfs_write(fd_write, &sensor_sample, SAMPLE_SIZE);
        if (write_bytes != SAMPLE_SIZE) 
          printf("[cfs] write: error writing into the 'WORKING_FILE'\n\n");
        else
          file_size += write_bytes;
                  
        cfs_close(fd_write);
        fd_write = EMPTY;
        
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
	mesh_open(&zoundtracker_conn, CHANNEL1, &zoundtracker_callbacks);                                             
	trickle_open(&zoundtracker_broadcast_conn, 0, CHANNEL2, &zoundtracker_broadcast_callbacks);       
	input_msg_type = output_msg_type = EMPTY;
	ack_timeout = 0;
	
    // CFS Initialization
    etimer_set(&control_timer, NUM_SECONDS_SAMPLE*CLOCK_SECOND);
    sample_number = 0;
    file_size = 0;
    fd_read = fd_write = EMPTY;
    //read_buffer[0] = 0;
    cfs_remove(WORKING_FILE);
    
    
    // Sensor Initialization
    accm_init();

    // State Initialization
    state = BLOCKED;
    
    printf("[state] current state 'BLOCKED'\n\n");
    
    // (!) Sending message ("HELLO_MN").
    // Changing to "DATA_SEND" from "BLOCKED" state  
    state = DATA_SEND;    
    printf("[state] current state 'DATA_SEND'\n\n");
    
    // 0. Send first "HELLO_MN"
    hello_msg();
    
    while (1)
    {
        PROCESS_WAIT_EVENT();
        if (ev == PROCESS_EVENT_TIMER)
        {
            printf("[event] timer expired event\n\n");
            
            if (state == BLOCKED)
            {
                // (!) Timer expired. 
                // Changing to "DATA_COLLECT" from "BLOCKED" state
                state = DATA_COLLECT;
                printf("[state] current state 'DATA_COLLECT'\n\n");
            
                get_sensor_sample();
                printf("[sensor] sample measured (sample number: %d)\n\n", sample_number); // 'sample_number' starts on zero
            
                // Updating state
                sample_number++;
            
                if (sample_number == 10)
                {    
                    // (!) We've got 10 sensor samples.
                    // Changing to "DATA_SEND" from "DATA_COLLECT" state
                    state = DATA_SEND;
                    printf("[state] current state 'DATA_SEND'\n\n");
                    
                    send_packet_from_file();
                    
                    sample_number = 0;                
                }
                else
                {
                    // (!) We've got less than 10 sensor samples.
                    // Changing to "BLOCKED" from "DATA_COLLECT" state
                    state = BLOCKED;
                    printf("[state] current state 'BLOCKED'\n\n");
                }
                  
            }
            else if (state == DATA_SEND)
            {
                if (ack_timeout == 1 && attempts >= MAX_ATTEMPTS)
                {
                    // ACK message lost. We can't erase the "WORKING_FILE"
                    file_send_failed();
                    
                    // Changing to "BLOCKED" from "DATA_SEND" state
                    state = BLOCKED;
                    printf("[state] current state 'BLOCKED'\n\n");
                }
                
            }       
        }
        else
          printf("[event] unknown event\n\n");  
    
        // 4. Reseting the timer 
        etimer_set(&control_timer, NUM_SECONDS_SAMPLE*CLOCK_SECOND);
    }
        
    PROCESS_END(); 
}
