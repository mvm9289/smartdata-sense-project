/* File: "zt-mn.c"
   Date (last rev.): 02/03/2011 11:57 AM                               */
   
#include "contiki.h"
#include "leds.h"
#include "cfs/cfs.h"
#include "contiki-lib.h"
#include "contiki-net.h"
#include "adxl345.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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
#define NUM_SECONDS_SAMPLE 3
#define WORKING_FILE "sample_file"
#define ERROR -1
#define NO_NEXT_PACKET -2

/* NET */
#define MAX_ATTEMPTS 5
#define MAX_FLOODING_ATTEMPTS 1
#define EMPTY -3
#define SEND_INTERVAL 10

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
static int attempts, flooding_attempts, packet_number, ack_waiting, 
    output_msg_type;
static struct uip_udp_conn *client_conn;
static unsigned char udp_stream[PACKET_SIZE], next_packet, 
    last_broadcast_id, valid_broadcast_id, num_msg_sended, num_msg_acked,
    num_files_sent;
static unsigned short packet_checksum;
static Packet packet_received;
/*static clock_time_t time_remaining;*/

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
    packet_to_send.macid = MACID;
    packet_to_send.type = HELLO_MN;
    packet_to_send.size = HELLO_MSG_SIZE;    
    packet_to_send.counter = HELLO_MSG_COUNTER;
    packet_to_send.checksum = compute_checksum(&packet_to_send);
    
    /* Net Control Information */
    packet_to_send.reserved[0] = (unsigned char)num_msg_sended;
    packet_to_send.reserved[1] = (unsigned char)num_msg_acked;
    packet_to_send.reserved[2] = (unsigned char)num_files_sent;
    
    
    /* Preparing "Packet" to send it through "rime". Building the 
       "rime_stream" using the information of "packet_to_send"   */
    mount_packet(&packet_to_send, udp_stream);
    
    /* "Packet" send to the "Basestation" */
    #ifdef DEBUG_NET
      printf("[net]\n Sending 'HELLO_MN' message\n\n");
    #endif

    /* Configuring type of message */
    output_msg_type = HELLO_MN;
    ack_waiting = TRUE;
    
    uip_udp_packet_send(client_conn, udp_stream, PACKET_SIZE);
    
    /* Net Control Information */
    num_msg_sended++;
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
    packet_to_send.macid = MACID;
    packet_to_send.type = DATA;
    packet_to_send.size = file_size;    
    packet_to_send.counter = (packet_number-1)*DATA_SIZE;
    memcpy(packet_to_send.data, read_buffer, read_bytes);
    packet_to_send.checksum = compute_checksum(&packet_to_send);
    
    /* Net Control Information */
    packet_to_send.reserved[0] = (unsigned char)num_msg_sended;
    packet_to_send.reserved[1] = (unsigned char)num_msg_acked;
    packet_to_send.reserved[2] = (unsigned char)num_files_sent;
    
    #ifdef DEBUG_NET
      printf("[net]\n 'DATA' packet contents\n");
      printf("  MacID: %x\n", packet_to_send.macid);
      printf("  Type: %d\n", packet_to_send.type);
      printf("  Size: %d\n", packet_to_send.size);
      printf("  Counter: %d\n", packet_to_send.counter);
      printf("  Data: %c\n", packet_to_send.data[0]);
      printf("  Checksum: %d\n\n", packet_to_send.checksum);
    #endif
    
    /* Preparing "Packet" to send it through "rime". Building the 
       "rime_stream" using the information of "packet_to_send"   */
    mount_packet(&packet_to_send, udp_stream);
    
    /* "Packet" send to the "Basestation" */

    #ifdef DEBUG_NET
      printf("[net]\n Sending 'DATA' message\n\n");
    #endif
    
    /* Configuring type of message */
    output_msg_type = DATA;
    ack_waiting = TRUE;
    
    uip_udp_packet_send(client_conn, udp_stream, PACKET_SIZE);
    
    /* Net Control Information */
    num_msg_sended++;
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

        num_files_sent++;
        sample_interval = 0;
        file_size = 0;

        #ifdef DEBUG_NET
          printf("[net]\n The 'WORKING_FILE' is completely sended,");
          printf(" removing it\n\n");
        #endif
    }
}

/* ------------------------------------------------------------------ */

static void tcpip_handler(void)
{
    //~ if (uip_acked())
    //~ {
        //~ #ifdef DEBUG_NET
          //~ printf("[net]\n Last UDP packet ACK received!!!!!!!!!!!!!!!!!!!!!\n\n");
        //~ #endif
    //~ }
    
    if(uip_newdata())
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
          printf("[net]\n Message received through 'UDP IPv6' connection\n\n");
        #endif
               
        /* Obtaining the "Packet" and checking checksum. */
        packet_received = unmount_packet((unsigned char*)uip_appdata);
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
                
                /* Saving the last broadcast_id as valid */
                valid_broadcast_id = last_broadcast_id;
                
                /* Net Control Information */
                num_msg_acked++;

                #ifdef DEBUG_NET
                  printf("[net]\n 'HELLO_ACK' received\n\n");
                #endif        

                leds_off(LEDS_GREEN);
                leds_off(LEDS_RED);
            }
            else if (packet_received.type == DATA_ACK)
            {	
                ack_waiting = FALSE;

                /* Net Control Information */
                num_msg_acked++;

                #ifdef DEBUG_NET
                  printf("[net]\n 'DATA_ACK' received\n\n");
                #endif
                
                leds_off(LEDS_GREEN);
                leds_off(LEDS_RED);
                
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
    
    if (uip_timedout())
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
}

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

static void set_connection_address(uip_ipaddr_t *ipaddr)
{
  // change this IP address depending on the node that runs the server!
  uip_ip6addr(ipaddr,0xfe80,0,0,0,0xc30c,0,0,0xff01);
}

/* ------------------------------------------------------------------ */

/* Process */
PROCESS(example_zoundt_mote_process, "Example zoundt mote process");
AUTOSTART_PROCESSES(&example_zoundt_mote_process);

PROCESS_THREAD(example_zoundt_mote_process, ev, data) {   
    
    PROCESS_BEGIN();

    /* NET */
    uip_ipaddr_t ipaddr;
    set_connection_address(&ipaddr);
    client_conn = udp_new(&ipaddr, 3000, NULL);
    output_msg_type = EMPTY;
    ack_waiting = FALSE;
    attempts = 0;
    num_msg_sended = 0;
    num_msg_acked = 0;
    num_files_sent = 0;
    
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
        if (ev == tcpip_event) tcpip_handler();
        else if (ev == PROCESS_EVENT_TIMER)
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
            
                if (sample_interval == SEND_INTERVAL)
                {    
                    #ifdef DEBUG_NET
                      printf("[net]\n");
                      printf(" Number of packets sended: %d\n", num_msg_sended);
                      printf(" Number of packets acknowledged: %d\n\n", num_msg_acked);
                      printf(" Number of files sent: %d\n\n", num_files_sent);
                    #endif
                
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
                    
                    attempts = 0;
                    
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
