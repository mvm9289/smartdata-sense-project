/* File: "zt-mn.c"
   Date (last rev.): 10/02/2011 7:39 AM                              */
   
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

/* ------------------------------------------------------------------ */

#ifdef TRUE
#undef TRUE
#endif
#define TRUE 1

#ifdef FALSE
#undef FALSE
#endif
#define FALSE 0

#ifdef DEBUG_MODE
#define DEBUG_NET
#define DEBUG_STATE
#define DEBUG_CFS
#define DEBUG_SENSOR
#define DEBUG_EVENT
#endif

/* CFS */
#define NUM_SECONDS_SAMPLE 6
#define WORKING_FILE "sample_file"
#define ERROR -1
#define NO_NEXT_PACKET -2

/* NET */
#define MAX_ATTEMPTS 5
#define EMPTY -3

/* Sensor */
#define SAMPLE_SIZE 2

/* State */
#define BLOCKED 1
#define DATA_COLLECT 2
#define DATA_SEND 3

/* ------------------------------------------------------------------ */

/* CFS */
static int write_bytes, read_bytes, fd_read, fd_write;
static unsigned short file_size;
static struct etimer control_timer;
static unsigned char read_buffer[DATA_SIZE];
static Sample current_sample;
    
/* NET */
static int attempts, packet_number, ack_waiting, output_msg_type;
static rimeaddr_t sink_addr;
static struct mesh_conn zoundtracker_conn;
static struct trickle_conn zoundtracker_broadcast_conn;
static unsigned char rime_stream[PACKET_SIZE], next_packet;
static unsigned short packet_checksum;

/* Sensor */
static char sensor_sample;
static int sample_interval;

/* State */
static unsigned char state;

/* ------------------------------------------------------------------ */

/* NET */
static void 
hello_msg() 
{
    /* [Functionality]
     This function builds and sends a "HELLO_MN" message to the 
     "Basestation". In this message there's the identificator that the 
     "Basestation" uses to register the node. 

       [Context]
     This message is sended to the "Basestation" when the node gets up. 
     However the message is sended to reply a broadcast "HELLO_BS" 
     message sended by the "Basestation" periodically, to execute the 
     "Discover" three-way handshake. */ 
    
    Packet packet_to_send;

    /* "Packet" construction. */
    packet_to_send.addr1 = MY_ADDR1;
    packet_to_send.addr2 = MY_ADDR2;
    packet_to_send.type = HELLO_MN;
    packet_to_send.size = HELLO_MSG_SIZE;    
    packet_to_send.counter = HELLO_MSG_COUNTER;
    packet_to_send.checksum = compute_checksum(&packet_to_send);
    
    /* Preparing "Packet" to send it through "rime". Building the 
       "rime_stream" using the information of "packet_to_send"   */
    mount_packet(&packet_to_send, rime_stream);
    packetbuf_copyfrom((void *)rime_stream, PACKET_SIZE);
	
	/* "Packet" send to the "Basestation" */
	sink_addr.u8[0] = SINK_ADDR1;
	sink_addr.u8[1] = SINK_ADDR2;

    #ifdef DEBUG_NET
	  printf("[net]\n Sending 'HELLO_MN' message\n\n");
    #endif

	/* Configuring type of message */
    output_msg_type = HELLO_MN;
	
	mesh_send(&zoundtracker_conn, &sink_addr);
}

static void 
data_msg() 
{   
    /* [Functionality]
     This function builds and sends a "DATA" message to the 
     "Basestation". In this message there's a set of sensor samples that 
     the node saved in the "WORKING_FILE". When all the sensor samples 
     are sended the "WORKING_FILE" is sended entirely. 

       [Context]
     This message is sended to the "Basestation" periodically every 10 
     minutes. However the "DATA" message is sended to reply a "POLL" 
     message sended by the "Basestation", to force the node to send it's 
     saved sensor samples. */

    Packet packet_to_send;
     
    /* "Packet" construction. */
    packet_to_send.addr1 = MY_ADDR1;
    packet_to_send.addr2 = MY_ADDR2;
    packet_to_send.type = DATA;
    packet_to_send.size = file_size;    
    packet_to_send.counter = (packet_number-1)*DATA_SIZE;
    memcpy(packet_to_send.data, read_buffer, read_bytes);
    packet_to_send.checksum = compute_checksum(&packet_to_send);
    
	#ifdef DEBUG_NET
	  printf("[net]\n 'DATA' packet contents\n");
	  printf("  Addr1: %d\n", packet_to_send.addr1);
	  printf("  Addr2: %d\n", packet_to_send.addr2);
	  printf("  Type: %d\n", packet_to_send.type);
	  printf("  Size: %d\n", packet_to_send.size);
	  printf("  Counter: %d\n", packet_to_send.counter);
	  printf("  Data: %c\n", packet_to_send.data[0]);
	  printf("  Checksum: %d\n\n", packet_to_send.checksum);
	#endif
	
    /* Preparing "Packet" to send it through "rime". Building the 
       "rime_stream" using the information of "packet_to_send"   */
    mount_packet(&packet_to_send, rime_stream);
    packetbuf_copyfrom((void *)rime_stream, PACKET_SIZE);
	
	/* "Packet" send to the "Basestation" */
	sink_addr.u8[0] = SINK_ADDR1;
	sink_addr.u8[1] = SINK_ADDR2;

	#ifdef DEBUG_NET
	  printf("[net]\n Sending 'DATA' message\n\n");
	#endif
    
	/* Configuring type of message */
    output_msg_type = DATA;
	
	mesh_send(&zoundtracker_conn, &sink_addr);
}

static unsigned char 
prepare_packet(void)
{
    /* [Functionally]
     This function reads the "WORKING_FILE" to get a set of sensor 
     samples for the next packet to send.

       [Context]
     This function is called from the function 'send_packet_from_file' 
     to check if there's more sensor samples to send in the 
     'WORKING_FILE".

       [Return value]
     The return value will be TRUE if 'read_bytes' is greater than 0.
     Otherwise the return value will be FALSE */


    /* Reading from the "WORKING_FILE". */
    if (fd_read == EMPTY)
    {
        /* That's the first packet of "WORKING_FILE". */
		#ifdef DEBUG_CFS
	      printf("[cfs]\n Trying to prepare the first packet of the");
	      printf(" 'WORKING_FILE'\n\n");
		#endif
        packet_number = 0;
        fd_read = cfs_open(WORKING_FILE, CFS_READ);
	}
	/* else currently sending the 'WORKING_FILE'. */
	
	if (fd_read == ERROR)
	{
		#ifdef DEBUG_CFS
	      printf("[cfs]\n Error openning the 'WORKING_FILE'");
	      printf(" for read data\n\n");
	    #endif
		return FALSE;
	}
	else 
	{                  
        read_bytes = cfs_read(fd_read, read_buffer, DATA_SIZE);
		#ifdef DEBUG_CFS
	      printf("[cfs]\n Num. bytes readed: %d\n\n", read_bytes);
		#endif
        
        if (read_bytes == ERROR)
        {
			#ifdef DEBUG_CFS
			  printf("[cfs]\n Error reading from the");
			  printf(" 'WORKING_FILE'\n\n");
			#endif
            return FALSE;  
        }
        else if (read_bytes == 0) {
          return FALSE;
        }
    }
    
    /* There's information to send. */
    return TRUE;
}

static void 
send_packet_from_file(void)
{
    /* [Functionality]
     This function checks if there's sensor samples to send at the 
     "WORKING_FILE" and if so, sends a "DATA" message.
     Otherwise, the file will be closed and removed and 'fd_read',
     'sample_interval' and 'file_size' will be reset.

       [Context]
     This function is called when we want to send the first packet of 
     the "WORKING_FILE" and when a "DATA_ACK" or "POLL" message is 
     received. */


    next_packet = prepare_packet();
          
    if (next_packet)
    {              
        /* "Packet" sending. */
        packet_number++;
        data_msg();
		
		#ifdef DEBUG_NET
		  printf("[net]\n Sending the 'WORKING_FILE'");
		  printf(" (packet number: %d)\n\n", packet_number);
		#endif
    }    
    else  
    {       
        /* 'WORKING_FILE' completely sended, removing it. */
        cfs_close(fd_read);
        fd_read = EMPTY;

        cfs_remove(WORKING_FILE);

        sample_interval = 0;
        file_size = 0;

		#ifdef DEBUG_NET
		  printf("[net]\n The 'WORKING_FILE' is completely sended,");
		  printf(" removing it\n\n");
        #endif
    }
}

/* ------------------------------------------------------------------ */
static void 
sent(struct mesh_conn *c) 
{
    /* [Functionality]
     This function resets the number of attempts to send the last 
     message and the control of "ACK" waiting is activated. This control
     consists in wait 1 minute for the 'ACK' reply. If the time is 
     exceed the 'ACK' reply is considered lost.

       [Context]
     This funcion is executed when a message sended to the "Basestation"
     arrives (radio message). But the integrity of the message could be 
     transgressed. */

    /* Checksum comprobation is needed on receiver. */
    
    attempts = 0;
    ack_waiting = TRUE;
    
    #ifdef DEBUG_NET
	  printf("[net]\n Sent message\n\n"); 
	#endif
        
    leds_on(LEDS_GREEN);
}

//----------------------------------------------------------------------
static void 
timedout(struct mesh_conn *c) 
{
    /* [Functionality]
     This function resends the currently sended message. If 
     "MAX_ATTEMPTS" is reached the message is considered lost. Then 
     resets the number of attempts and returns to the "BLOCKED" state to 
     start another sample period.

       [Context]
     This function is executed when there's a radio lost message. The
     function tries again "MAX_ATTEMPTS" attempts. If the "Basestation" 
     don't receive any message the file send is considered failed. The 
     file is closed (data is not erased, must be sended on the next 
     send) and the node starts another sample period. */


    attempts++;
	if (attempts < MAX_ATTEMPTS) 
	{
	    if (output_msg_type == HELLO_MN)
	    {
	        /* Resending "HELLO_MN" message. */
	        hello_msg();
	        
			#ifdef DEBUG_NET
			  printf("[net]\n Timeout resending 'HELLO_MN' message\n\n");
			#endif
	    }
	    else if (output_msg_type == DATA)
	    {
	        /* Resending "DATA" message. */
	        data_msg();
        
            #ifdef DEBUG_NET
		      printf("[net]\n Timeout resending the current packet");
		      printf(" (packet number: %d)", packet_number);
		      printf(" from the 'WORKING_FILE'\n\n");
			#endif
        }
	}
	else
	{
	    #ifdef DEBUG_NET
		  printf("[net]\n Maximum number of attempts reached\n");
		#endif
	    if (output_msg_type == HELLO_MN)
		{	    
		    #ifdef DEBUG_NET
			  printf("[net]\n 'HELLO_MN' message lost\n\n");
			#endif
	    }
	    else if (output_msg_type == DATA)
		{	
            /* Current sending message lost. We can't erase the 
               "WORKING_FILE". */
            cfs_close(fd_read);
            fd_read = EMPTY;       

            /* Starting new sample period (10 minutes). */
            sample_interval = 0;
        
		    #ifdef DEBUG_NET
			  printf("[net]\n 'DATA' message lost");
			  printf(" (packet number: %d)\n\n", packet_number);
			#endif
        }
                
        /* (!) Message lost.
           Changing to "BLOCKED" from "DATA_SEND" state. */  
        state = BLOCKED;
        
        #ifdef DEBUG_STATE
	      printf("---\n[state]\n Current state 'BLOCKED'\n---\n\n");
	    #endif
	    
	    leds_off(LEDS_BLUE);

        attempts = 0;
        
        leds_on(LEDS_RED);
	}
}

//----------------------------------------------------------------------
static void 
received(struct mesh_conn *c, const rimeaddr_t *from, uint8_t hops) 
{
    /* [Functionality]
     This functions checks the type of message received and acts 
     consequently. First the checksum's message is checked, if it's 
     correct then attend the message by the message type. If there's no
     pending net work (means that, the last message is an 'ACK' and the
     'WORKING_FILE' is not opened for sending), the node returns to 
     'BLOCKED' state.
     
       [Context]
     This function is executed when a message is received through the
     mesh connection. */
  
    
    #ifdef DEBUG_NET
	  printf("[net]\n Message received trough 'mesh' connection\n\n");
	#endif
           
    /* Obtaining the "Packet" and checking checksum. */
    Packet packet_received;
    packet_received = unmount_packet(packetbuf_dataptr());
    packet_checksum = compute_checksum(&packet_received);

    if (packet_checksum == packet_received.checksum)
    {
        /* (!) Message received ("POLL/HELLO_ACK/DATA_ACK").
            Changing to "DATA_SEND" from "DATA_SEND/BLOCKED/DATA_COLLECT"
            state. */ 
        state = DATA_SEND;
        
        #ifdef DEBUG_STATE
		  printf("---\n[state]\n Current state 'DATA_SEND'\n---\n\n");
		#endif
		
		leds_on(LEDS_BLUE);
    
        if (packet_received.type == HELLO_ACK)
        {
            ack_waiting = FALSE;
        
    		#ifdef DEBUG_NET
    		  printf("[net]\n 'HELLO_ACK' received\n\n");
    		#endif        

            leds_off(LEDS_GREEN);
            leds_off(LEDS_RED);
        }
        else if (packet_received.type == DATA_ACK)
        {	
            ack_waiting = FALSE;

    		#ifdef DEBUG_NET
    		  printf("[net]\n 'DATA_ACK' received\n\n");
    		#endif
            
            /* If it's necessary sends next packet. */           
            send_packet_from_file();        
        }
        else if (packet_received.type == POLL)
        {
    		#ifdef DEBUG_NET
    		  printf("[net]\n 'POLL' received\n\n");
    		#endif

            if (fd_read != EMPTY)
            {
                /* "POLL" message on reply to a "DATA" message 
                    sended and lost. Resending the last packet. */
                data_msg();
            }
            else
            {
                /* "POLL" message advertisement. Node timestamp 
                    exceed. Sending the "WORKING_FILE". */
                send_packet_from_file();
            }
        }
        else 
        {
            #ifdef DEBUG_NET
	          printf("[net]\n Incorrect type of message received\n\n");
		    #endif
        }
        
        if (ack_waiting == FALSE && fd_read == EMPTY)
        {
            /* We're not pending for an 'ACK' message and the 
            'WORKING_FILE' is not opened for sending. 
            Net work finalized */
            state = BLOCKED;
            
            #ifdef DEBUG_STATE
		      printf("---\n[state]\n Current state 'BLOCKED'\n---\n\n");
		    #endif
		    
		    leds_off(LEDS_BLUE);
        }
    }
    else 
    {
        #ifdef DEBUG_NET
	      printf("[net]\n Incorrect checksum invalid message\n\n");
		#endif
    }
}

static void 
broadcast_received(struct trickle_conn* c)
{
    /* [Functionality]
     The behaviour of this function is very similar to the "received" 
     "mesh" callback to attend only the "HELLO_BS" messages sended 
     through the "trickle" broadcast connection. This function replies 
     the "HELLO_BS" messages sending a "HELLO_MN" message to the 
     "Basestation". If there's already a data sending, the "HELLO_BS" 
     message is discarded.

       [Context]
     This function sends a "HELLO_MN" message through the "mesh" 
     connection when a "broadcast" "HELLO_BS" message received from the 
     "Basestation". */
    
    
    #ifdef DEBUG_NET
	  printf("[net]\n Message received trough 'trickle' connection\n\n");
	#endif
    
    if (state != DATA_SEND)
    {    
    
        /* Obtaining the "Packet" and checking checksum. */
        Packet packet_received;
        packet_received = unmount_packet(packetbuf_dataptr());
        packet_checksum = compute_checksum(&packet_received);
        
        if (packet_checksum == packet_received.checksum)
        {
            /* (!) Message received ("HELLO_BS").
               Changing to "DATA_SEND" from 
               "BLOCKED/DATA_COLLECT" state. */  
            state = DATA_SEND;
            
            #ifdef DEBUG_STATE
			  printf("---\n[state]\n Current state 'DATA_SEND'\n---\n\n");
			#endif
			
			leds_on(LEDS_BLUE);
            
            if (packet_received.type == HELLO_BS)
            {
                #ifdef DEBUG_NET
    			  printf("[net]\n 'HELLO_BS' message received\n\n");
                #endif
			
                /* Sending "HELLO_MN" message. */
                hello_msg();
            }
            else
            {
                #ifdef DEBUG_NET
    	          printf("[net]\n Incorrect type of message received\n\n");
    		    #endif
            }
        }
        else 
        {
            /* Invalid message. */
            #ifdef DEBUG_NET
			  printf("[net]\n Incorrect checksum invalid message\n\n");
			#endif
        }
    }
    else {
        #ifdef DEBUG_NET
	      printf("[net]\n Already sending a message.");
	      printf(" Message received discarded.\n\n");
		#endif
        
    }
}

const static struct mesh_callbacks 
  zoundtracker_callbacks = {received, sent, timedout};
const static struct trickle_callbacks 
  zoundtracker_broadcast_callbacks = {broadcast_received};

/* ------------------------------------------------------------------ */

/* Sensor */
void 
get_sensor_sample(void)
{
    /* [Functionality]
     This functions takes a sample of the x axis from the accelerometer 
     sensor. Then stores the pair (number of sample, sensor sample) into 
     the "WORKING_FILE". If there's any error the "WORKING_FILE" is not 
     modified. By this way we can associate every sample to every minute 
     on the sample period.

       [Context]
     This function is executed when the node enters into "DATA_COLLECT" 
     state to take a sensor sample, every sample period. */
     	
	
	/* Reading data from sensor. */
	sensor_sample = (char)accm_read_axis(X_AXIS);
	
	#ifdef DEBUG_SENSOR
	  printf("[accel]\n Axis: x\n Value: %d\n\n", sensor_sample);
	#endif

    /* Building the 'Sample' structure for writing. */
    current_sample.number = sample_interval;
    current_sample.value = sensor_sample;

    /* Writing data into the "WORKING_FILE". */
	fd_write = cfs_open(WORKING_FILE, CFS_WRITE | CFS_APPEND);       
    if (fd_write == ERROR) 
	{	
	    #ifdef DEBUG_CFS
		  printf("[cfs]\n Error openning the 'WORKING_FILE'");
		  printf(" for write data\n\n");
		#endif
    }
    else 
    {
        write_bytes = cfs_write(fd_write, &current_sample, SAMPLE_SIZE);
        if (write_bytes != SAMPLE_SIZE) 
		{
			#ifdef DEBUG_CFS
			  printf("[cfs]\n Write: error writing into the");
			  printf(" 'WORKING_FILE'\n\n");
			#endif
        }
        else
          file_size += write_bytes;
                  
        cfs_close(fd_write);
        fd_write = EMPTY;
        
    }
}
/* ------------------------------------------------------------------ */

/* Process */
PROCESS(example_zoundt_mote_process, "Example zoundt mote process");
AUTOSTART_PROCESSES(&example_zoundt_mote_process);

PROCESS_THREAD(example_zoundt_mote_process, ev, data) {   
    
    PROCESS_EXITHANDLER(mesh_close(&zoundtracker_conn);)
    PROCESS_BEGIN();
    
    /* NET */
    rimeaddr_t my_addr;
	my_addr.u8[0] = MY_ADDR1;
	my_addr.u8[1] = MY_ADDR2;
	rimeaddr_set_node_addr(&my_addr);
	mesh_open(&zoundtracker_conn, CHANNEL1, &zoundtracker_callbacks);                                             
	trickle_open(&zoundtracker_broadcast_conn, 0, CHANNEL2, 
	  &zoundtracker_broadcast_callbacks);       
	output_msg_type = EMPTY;
	ack_waiting = FALSE;
	attempts = 0;
	
    /* CFS */
    etimer_set(&control_timer, NUM_SECONDS_SAMPLE*CLOCK_SECOND);
    sample_interval = 0;
    file_size = 0;
    fd_read = fd_write = EMPTY;
    cfs_remove(WORKING_FILE);
    
    /* Sensor */
    accm_init();

    /* State */
    state = BLOCKED;
    
    #ifdef DEBUG_STATE
	  printf("---\n[state]\n Current state 'BLOCKED'\n---\n\n");
	#endif
	
	leds_off(LEDS_BLUE);
    
    /* (!) Sending message ("HELLO_MN").
       Changing to "DATA_SEND" from "BLOCKED" state. */  
    state = DATA_SEND;    
    
    #ifdef DEBUG_STATE
	  printf("---\n[state]\n Current state 'DATA_SEND'\n---\n\n");
	#endif
    
    leds_on(LEDS_BLUE);
    
    /* Sending first "HELLO_MN" message. */
    hello_msg();
    
    while (1)
    {
        PROCESS_WAIT_EVENT();
        if (ev == PROCESS_EVENT_TIMER)
        {
            #ifdef DEBUG_EVENT
			  printf("[event]\n Timer expired event\n\n");
			#endif
            
            if (state == BLOCKED)
            {
                /* (!) Timer expired. 
                   Changing to "DATA_COLLECT" from "BLOCKED" state. */
                state = DATA_COLLECT;
                
                #ifdef DEBUG_STATE
				  printf("---\n[state]\n");
				  printf(" Current state 'DATA_COLLECT'\n---\n\n");
				#endif
            
                leds_off(LEDS_BLUE);
            
                get_sensor_sample();
                
                #ifdef DEBUG_SENSOR
				  printf("[sensor]\n Sample measured");
				  printf(" (sample number: %d)\n\n", sample_interval); 
				#endif
                /* 'sample_interval' starts on zero. */
                
                /* Updating state. */
                sample_interval++;
            
                if (sample_interval == 10)
                {    
                    /* (!) We've got 10 sensor samples.
                       Changing to "DATA_SEND" from "DATA_COLLECT" 
                       state. */
                    state = DATA_SEND;
                    
                    #ifdef DEBUG_STATE
					  printf("---\n[state]\n");
					  printf(" Current state 'DATA_SEND'\n---\n\n");
					#endif
                    
                    leds_on(LEDS_BLUE);
                    
                    send_packet_from_file();
                    
                    sample_interval = 0;                
                }
                else
                {
                    /* (!) We've got less than 10 sensor samples.
                       Changing to "BLOCKED" from "DATA_COLLECT" 
                       state. */
                    state = BLOCKED;
                    
                    #ifdef DEBUG_STATE
					  printf("---\n[state]\n");
					  printf(" Current state 'BLOCKED'\n---\n\n");
					#endif
					
					leds_off(LEDS_BLUE);
                }
                  
            }
            else if (state == DATA_SEND)
            {
                if (ack_waiting == TRUE)
                {
                    if (output_msg_type == DATA)
                    {
                        #ifdef DEBUG_NET
    					  printf("[net]\n 'DATA_ACK' message lost\n\n");
    					#endif
                        /* ACK message lost. We can't erase the 
                           "WORKING_FILE". */
                        cfs_close(fd_read);
                        fd_read = EMPTY;       

                        ack_waiting = FALSE;
     
                        /* Starting a new sample period */
                        sample_interval = 0;
                        
                        /* Changing to "BLOCKED" from "DATA_SEND" 
                           state. */
                        state = BLOCKED;
    					
    					#ifdef DEBUG_STATE
    					  printf("---\n[state]\n");
    					  printf(" current state 'BLOCKED'\n---\n\n");
    					#endif
    					
    					leds_off(LEDS_BLUE);
                    }
                    else if (output_msg_type == HELLO_MN)
                    {
                        #ifdef DEBUG_NET
    					  printf("[net]\n 'HELLO_ACK' message lost\n\n");
    					#endif
    					
    					ack_waiting = FALSE;
                        
                        /* Changing to "BLOCKED" from "DATA_SEND" 
                           state. */
                        state = BLOCKED;
    					
    					#ifdef DEBUG_STATE
    					  printf("---\n[state]\n");
    					  printf(" Current state 'BLOCKED'\n---\n\n");
    					#endif  
    					
    					leds_off(LEDS_BLUE);             
                    }
                }
            }       
        }
        else
		{	
			#ifdef DEBUG_EVENT
			  printf("[event]\n Unknown event\n\n");  
			#endif
		}
        /* Reseting the timer. */ 
        etimer_set(&control_timer, NUM_SECONDS_SAMPLE*CLOCK_SECOND);
    }
        
    PROCESS_END(); 
}
