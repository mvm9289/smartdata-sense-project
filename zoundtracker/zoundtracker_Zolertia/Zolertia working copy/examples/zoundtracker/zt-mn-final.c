// File: "zt-mn-second.c"
// Date (last rev.): 31/07/2011
   
#include "contiki.h"
#include "leds.h"
#include "cfs/cfs.h"
#include "net/rime.h"
#include "net/rime/mesh.h"
#include "net/rime/broadcast.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Sensors

#include "dev/battery-sensor.h"
#include "dev/zt-phidget.h"

// Packet manager
#include "lib/zt-packet-mgmt.h"

// File Manager
#include "lib/zt-filesys-lib.h"

// Debug lib
#include "lib/zt-debug-lib.h"

//--------------------------------------------------------------------

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

#ifdef DEBUG_ZOL
#define DEBUG_NET
#define DEBUG_SENSOR
#endif

#define ZT_FINAL_VERSION
#define MY_ADDR1 0
#define MY_ADDR2 70

 
// CFS
#define NUM_SECONDS_SAMPLE 10  // Default 60 (1 sample/min)
#define WORKING_FILE "sample_file"
#define ERROR -1
#define NO_NEXT_PACKET -2
#define MAX_READ_ATTEMPTS 5
#define MAX_WRITE_ATTEMPTS 5
#define ERROR_CFS -7

// NET 
#define MAX_ATTEMPTS 5
#define MAX_FLOODING_ATTEMPTS 1
#define EMPTY -3
#define SEND_INTERVAL 10

// Sensor
#define SAMPLE_SIZE 3
#define SAMPLE_TIMER CLOCK_SECOND/100
#define SAMPLE_NUMBER (8000*2)

// State
#define BLOCKED 1
#define DATA_COLLECT 2
#define DATA_SEND 3

//--------------------------------------------------------------------

// CFS
static FileManager fmanLocal;
static FileManager fmanNet;
static int write_bytes, read_bytes;
static struct etimer control_timer;
static unsigned char read_buffer[DATA_SIZE];
static Sample current_sample;
static char initOkLocal;
static char initOkNet;
static int read_attempts;
static int write_attempts;

    
// NET
static int attempts, flooding_attempts, packet_number, ack_waiting, 
    output_msg_type;
static rimeaddr_t sink_addr;
static struct mesh_conn zoundtracker_conn;
static struct broadcast_conn zoundtracker_broadcast_conn;
static unsigned char rime_stream[PACKET_SIZE], next_packet, 
    last_broadcast_id, valid_broadcast_id, num_msg_sended, num_msg_acked;
static unsigned short packet_checksum;
static Packet packet_received, ack_packet;
// static clock_time_t time_remaining;

// Sensor
static int sample_interval;
static struct etimer samp_timer;
static unsigned int sampcont = 0;
static unsigned int sampref = 0;  
static unsigned long int holdbuf = 0;


// State
static unsigned char state;

//--------------------------------------------------------------------

// NET
static void 
hello_msg() 
{
     
    // Function: This function builds and sends a "HELLO_MN" message 
    // to the "Basestation". In this message there's the identificator 
    // that the "Basestation" uses to register the node. 
       
    // Context: This message is sended to the "Basestation" when the 
    // node gets up.  
    
    Packet packet_to_send;

    // Packet construction
    packet_to_send.addr1 = MY_ADDR1;
    packet_to_send.addr2 = MY_ADDR2;
    packet_to_send.type = HELLO_MN;
    packet_to_send.size = HELLO_MSG_SIZE;    
    packet_to_send.counter = HELLO_MSG_COUNTER;
    packet_to_send.checksum = compute_checksum(&packet_to_send);
    
    // Net Control Information
    packet_to_send.reserved[0] = (unsigned char)num_msg_sended;
    packet_to_send.reserved[1] = (unsigned char)num_msg_acked;
    
    
    // Preparing packet to send it through "rime". Building the 
    // "rime_stream" using the information of "packet_to_send"
    mount_packet(&packet_to_send, rime_stream);
    packetbuf_copyfrom((void *)rime_stream, PACKET_SIZE);

    // Packet destination: "Basestation"
    sink_addr.u8[0] = SINK_ADDR1;
    sink_addr.u8[1] = SINK_ADDR2;

    #ifdef DEBUG_NET
      debug_net_sending_message("HELLO_MN");
    #endif

    // Type of the last message sent
    output_msg_type = HELLO_MN;
	
    mesh_send(&zoundtracker_conn, &sink_addr);
	
    // Net Control Information
    num_msg_sended++;
}

static void 
data_msg() 
{   
    // Function: This function builds and sends a "DATA" message to the 
    // "Basestation". In this message there's a set of sensor samples 
    // that the node saved in a file on the Filesystem.

    // Context: This message is sent to the "Basestation" periodically 
    // every NUM_SECONDS_SAMPLE*SEND_INTERVAL seconds. However the 
    // "DATA" message is sent to reply a "POLL" message sent by the 
    // "Basestation".

    // Disable other ADC channel
    SENSORS_DEACTIVATE(ztalog);

    Packet packet_to_send;
     
    // "Packet" construction
    packet_to_send.addr1 = MY_ADDR1;
    packet_to_send.addr2 = MY_ADDR2;
    packet_to_send.type = DATA;
    packet_to_send.size = DATA_SIZE;    
    packet_to_send.counter = (packet_number-1)*DATA_SIZE;
    memcpy(packet_to_send.data, read_buffer, read_bytes);
    packet_to_send.checksum = compute_checksum(&packet_to_send);
    
    // Net Control Information
    packet_to_send.reserved[0] = (unsigned char)num_msg_sended;
    packet_to_send.reserved[1] = (unsigned char)num_msg_acked;

    // Adding battery sensor reading
    SENSORS_ACTIVATE(battery_sensor);
    uint16_t batt = battery_sensor.value(0);

    SENSORS_DEACTIVATE(battery_sensor);
    SENSORS_ACTIVATE(ztalog);

    packet_to_send.reserved[18] = (unsigned char) ((batt & 0xFF00) >> 8);
    packet_to_send.reserved[19] = (unsigned char) (batt & 0x00FF);
    
    #ifdef DEBUG_NET
      debug_net_packet_content(&packet_to_send);
    #endif
	
    // Preparing packet to send it through "rime". Building the 
    // "rime_stream" using the information of "packet_to_send"
    mount_packet(&packet_to_send, rime_stream);
    packetbuf_copyfrom((void *)rime_stream, PACKET_SIZE);
	
    // Packet send to the "Basestation"
    sink_addr.u8[0] = SINK_ADDR1;
    sink_addr.u8[1] = SINK_ADDR2;

    #ifdef DEBUG_NET
      debug_net_sending_message("DATA");
    #endif
    
    // Type of the last message sent
    output_msg_type = DATA;
	
    mesh_send(&zoundtracker_conn, &sink_addr);
	
    // Net Control Information
    num_msg_sended++;
}


static void ACK_msg(int type) 
{
    // Packet construction
    ack_packet.addr1 = MY_ADDR1;
    ack_packet.addr2 = MY_ADDR2;
    ack_packet.type = type;
    ack_packet.size = 0;    
    ack_packet.counter = 0;
    ack_packet.checksum = compute_checksum(&ack_packet);
}


static unsigned char 
prepare_packet(void)
{
    // Function: This function reads one file from the Filesystem to 
    // get a set of sensor samples for the next packet to send.

    // Context: This function is called from the function 
    // 'send_packet_from_file' to check if there's more sensor samples 
    // to send in the Filesystem.

    // Return: The return value will be TRUE if 'read_bytes' is 
    // greater than 0, otherwise the returns FALSE

    int pos;
    if (read_attempts > 0) 
    {
        pos = readSeek(&fmanLocal, START_POSITION);
        if(pos == ERROR_INVALID_FD) 
        {
            ++read_attempts;
            if(read_attempts < MAX_READ_ATTEMPTS)return prepare_packet();
            else 
            {
                read_attempts = 0;
                return ERROR_INVALID_FD;
            }
        }
        else if (pos == ERROR_READ_SEEK) {
            read_attempts = 0;
            return ERROR_READ_SEEK;
        }
    }
    
    read_bytes = read(&fmanLocal, read_buffer, DATA_SIZE);
    if (read_bytes > 0) 
    {
        read_attempts = 0;
        return TRUE;
    }
    else if(read_bytes == ERROR_READ_FILE) 
    {
        read_attempts = 0;
        return ERROR_READ_FILE;
    }
    else if(read_bytes == ERROR_INVALID_FD) 
    {
        ++read_attempts;
        if(read_attempts < MAX_READ_ATTEMPTS)return prepare_packet();
        else 
        {
            read_attempts = 0;
            return ERROR_INVALID_FD;
        }
    }
    else if (read_bytes == 0) return FALSE;
    else return ERROR_CFS;
}

static void 
send_packet_from_file(void)
{
    // Function: This function checks if there's sensor samples to 
    // send in the Filesystem and if so, sends a "DATA" message.
    
    // Context: This function is called when we want to send the first 
    // packet from a file, and when a "DATA_ACK" or "POLL" message is 
    // received.

      int storedFiles = getStoredFiles(&fmanLocal);
	
      next_packet = prepare_packet();
	      
      if (next_packet == TRUE)
      {              
          // Packet sending
          // packet_number++;
          data_msg();
	
          #ifdef DEBUG_NET
            debug_net_sending_WORKING_FILE(packet_number);
          #endif
      }    
      else if (next_packet == FALSE) 
      {       
          sample_interval = 0;
          updateReadFile(&fmanLocal);
          storedFiles = getStoredFiles(&fmanLocal);
          if (storedFiles > 0) send_packet_from_file();
      }
}

//--------------------------------------------------------------------

static void 
sent(struct mesh_conn *c) 
{
    // Function: This function resets the number of attempts to send 
    // the last message and the control of "ACK" waiting is activated. 
    // This control consists in wait one minute for the 'ACK' reply. 
    // If the time is exceed the 'ACK' reply is considered lost.

    // Context: This funcion is executed when a message sent to the 
    // "Basestation" arrives (radio message), but the integrity of the 
    // message could be transgressed.

    // Checksum comprobation is needed on receiver
    attempts = 0;
    if(output_msg_type == DATA || output_msg_type == HELLO_MN)
    	ack_waiting = TRUE;
    
    #ifdef DEBUG_NET
      debug_net_sent_message();
    #endif    
}

//----------------------------------------------------------------------
static void 
timedout(struct mesh_conn *c) 
{
    // Function: This function resends the currently sent message. 
    // If "MAX_ATTEMPTS" is reached the message is considered lost. 
    // Then resets the number of attempts and returns to the "BLOCKED" 
    // state to start another sample period.

    // Context: This function is executed when there's a radio lost 
    // message. The function tries again "MAX_ATTEMPTS" attempts. If 
    // the "Basestation" don't receive any message the file sent is 
    // considered lost.

    attempts++;
    if (attempts < MAX_ATTEMPTS) 
    {
        if (output_msg_type == HELLO_MN)
        {
            // Resending "HELLO_MN" message
            hello_msg();
	        
            #ifdef DEBUG_NET
              debug_net_timeout(HELLO_MN, 0);
            #endif
        }
        else if (output_msg_type == DATA)
        {
            // Resending "DATA" message.
            data_msg();

            #ifdef DEBUG_NET
              debug_net_timeout(DATA, packet_number);
            #endif
        }
    }
    else
    {
        #ifdef DEBUG_NET
          debug_net_max_attempts();
        #endif
        if (output_msg_type == HELLO_MN)
        {	    
            #ifdef DEBUG_NET
              debug_net_message_lost(HELLO_MN,0);
            #endif
        }
        else if (output_msg_type == DATA)
        {	
            // Current sending message lost. Starting new sample period.
            sample_interval = 0;
            
            readSeek(&fmanLocal, START_POSITION);
        
            #ifdef DEBUG_NET
              debug_net_message_lost(DATA,packet_number);
            #endif
        }
                
        // Message lost. Changing to "BLOCKED" from "DATA_SEND" state.  
        state = BLOCKED;
        
        #ifdef DEBUG_STATE
          debug_state_current_state("BLOCKED");
        #endif

        attempts = 0;
    }
}

//--------------------------------------------------------------------

static void 
received(struct mesh_conn *c, const rimeaddr_t *from, uint8_t hops) 
{
    // Function: This functions checks the type of message received 
    // and acts consequently. First the checksum's message is checked, 
    // if it's correct then attends the message by the message type. 
    // If there's no pending net work the node returns to 'BLOCKED' 
    // state.
     
    // Context: This function is executed when a message is received 
    // through the mesh connection.

    unsigned char prev_state = state;
    int storedFiles,i;

    #ifdef DEBUG_NET
      debug_net_message_received_connection("mesh");
    #endif
           
    // Obtaining the packet and checking checksum.
    packet_received = unmount_packet(packetbuf_dataptr());
    packet_checksum = compute_checksum(&packet_received);

    if (packet_checksum == packet_received.checksum)
    {
        // Message received ("POLL/HELLO_ACK/DATA_ACK"). Changing to 
        // "DATA_SEND" from "DATA_SEND/BLOCKED/DATA_COLLECT" state. 
        state = DATA_SEND;
        
        #ifdef DEBUG_STATE
          debug_state_current_state("DATA_SEND");
        #endif
		
        if (packet_received.type == HELLO_ACK)
        {
            ack_waiting = FALSE;
            
            // Saving the last broadcast_id as valid
            valid_broadcast_id = last_broadcast_id;
            
            // Net Control Information
            num_msg_acked++;

            #ifdef DEBUG_NET
              debug_net_message_received("HELLO_ACK");
            #endif        
        }
        else if (packet_received.type == DATA_ACK)
        {	
            ack_waiting = FALSE;

            // Net Control Information
            num_msg_acked++;

            #ifdef DEBUG_NET
              debug_net_message_received("DATA_ACK");
            #endif

            // Trying to send the next packet           
            send_packet_from_file();        
        }
        else if (packet_received.type == POLL)
        {
            #ifdef DEBUG_NET
              debug_net_message_received("POLL");
            #endif

            if (prev_state == DATA_SEND)
            {
                // "POLL" message on reply to a "DATA" message sent 
                // and lost. Resending the last packet.
                data_msg();
            }
            else
            {
                // "POLL" message advertisement. Node timestamp exceed. 
                // Sending the stored files.
                send_packet_from_file();
            }
        }
        else if(packet_received.type == HELLO_MN)
        {
            #ifdef DEBUG_NET
              debug_net_message_received("HELLO_MN");        		
            #endif
        	  
            // Mounting ACK message
            ACK_msg(HELLO_ACK);

            // Preparing "Packet" to send it through "rime". Building 
            // the "rime_stream" using the information of "ack_packet"   
            mount_packet(&ack_packet, rime_stream);
            packetbuf_copyfrom((void *)rime_stream, PACKET_SIZE);
	
            #ifdef DEBUG_NET
              debug_net_sending_message("HELLO ACK");
            #endif

            // Type of the last message sent
            output_msg_type = HELLO_ACK;

            mesh_send(&zoundtracker_conn, from);

        }
        else if(packet_received.type == DATA)
        {
            // DATA message received. Resending to the SINK

            // rssi = cc2420_rssi();
            // packet_received.reserved[2] = (char)(rssi>>8);
            // packet_received.reserved[3] = (char)(rssi);
            #ifdef DEBUG_NET
                printf("             Mesh received callback: ");
                printf("Data packet received from %d.%d\n\n",
                        from->u8[0],
                        from->u8[1]);
                printf("             Mesh received callback: ");
                printf("Packet content:\n");
                printf("             MOBILE NODE ID: %d.%d\n",
                        packet_received.addr1,
                        packet_received.addr2);
                printf("             MESSAGE TYPE: %d\n", packet_received.type);
                printf("             SIZE: %d\n", packet_received.size);
                printf("             COUNTER: %d\n", packet_received.counter);
                printf("             DATA: ");
                
                for (i = 0; i < DATA_SIZE &&
                  packet_received.counter + i < packet_received.size;
                  i++)
                    printf("%d ", packet_received.data[i]);
                
                printf("             \n");
                // printf("             RSSI: %d\n", rssi);
                printf("             HOPS: %d\n", hops);
                printf("             NUMBER OF PACKETS SENDED: %d\n",
                        packet_received.reserved[0]);
                printf("             NUMBER OF PACKETS ACKNOWLEDGED: %d\n",
                        packet_received.reserved[1]);
                printf("             CHECKSUM: %d\n\n",
                        packet_received.checksum);
            #endif
            
            // Mounting ACK message
            ACK_msg(DATA_ACK);

            // Preparing "Packet" to send it through "rime". Building 
            // the "rime_stream" using the information of "ack_packet"   
            mount_packet(&ack_packet, rime_stream);
            packetbuf_copyfrom((void *)rime_stream, PACKET_SIZE);
	
            #ifdef DEBUG_NET
              debug_net_sending_message("DATA ACK");
            #endif

            // Type of the last message sent
            output_msg_type = DATA_ACK;
	
            mesh_send(&zoundtracker_conn, from);

            // Saving the packet into the net filesystem
            i = write(&fmanNet, packet_received.data,DATA_SIZE);
            printf("%d bytes written\n",i);
                
            // Net Control Information
            num_msg_sended++;
        }
        else 
        {
            #ifdef DEBUG_NET
              debug_net_message_received("incorrect");
            #endif
        }
        
        storedFiles = getStoredFiles(&fmanLocal);
        
        if ((ack_waiting == FALSE && storedFiles==0) || 
             output_msg_type == DATA_ACK)
        {
            // Checking stored files to be send.
            if(getStoredFiles(&fmanNet) > 0) 
            {
                read(&fmanNet,read_buffer,DATA_SIZE);
                // packetbuf_copyfrom((void *)read_buffer, PACKET_SIZE);
               
                // Packet send to the "Basestation"
                /*sink_addr.u8[0] = SINK_ADDR1;
                sink_addr.u8[1] = SINK_ADDR2;

                #ifdef DEBUG_NET
                  debug_net_sending_message("DATA (FORWARD)");
                #endif
                
                // Type of the last message sent
                output_msg_type = DATA;
	
                mesh_send(&zoundtracker_conn, &sink_addr);
	
                // Net Control Information
                num_msg_sended++;*/
	            
	            
                // Modificar funcion data_msg() para permitir enviar 
                // mensajes del net filesystem!!!!!!!!!!!!!!!!!!!!!!
                SENSORS_DEACTIVATE(ztalog);

                Packet packet_to_send;
				 
                // Packet construction.
                packet_to_send.addr1 = 30;
                packet_to_send.addr2 = 9;
                packet_to_send.type = DATA;
                packet_to_send.size = DATA_SIZE;    
                packet_to_send.counter = 0;
                memcpy(packet_to_send.data, read_buffer, DATA_SIZE);
                packet_to_send.checksum = compute_checksum(&packet_to_send);

                // Net Control Information
                packet_to_send.reserved[0] = (unsigned char)num_msg_sended;
                packet_to_send.reserved[1] = (unsigned char)num_msg_acked;

                // Adding battery sensor reading
                SENSORS_ACTIVATE(battery_sensor);
                uint16_t batt = battery_sensor.value(0);
		
                #ifdef DEBUG_NET
                  debug_net_battery(batt);
                #endif

                SENSORS_DEACTIVATE(battery_sensor);
                SENSORS_ACTIVATE(ztalog);

                packet_to_send.reserved[18] = (unsigned char) ((batt & 0xFF00) >> 8);
                packet_to_send.reserved[19] = (unsigned char) (batt & 0x00FF);
		
                #ifdef DEBUG_NET
                  debug_net_packet_content(&packet_to_send);
                #endif
	
                // Preparing packet to send it through "rime". Building 
                // the "rime_stream" using the information of 
                // "packet_to_send".
                mount_packet(&packet_to_send, rime_stream);
                packetbuf_copyfrom((void *)rime_stream, PACKET_SIZE);
	
                // "Packet" send to the "Basestation"
                sink_addr.u8[0] = SINK_ADDR1;
                sink_addr.u8[1] = SINK_ADDR2;

                #ifdef DEBUG_NET
                  debug_net_sending_message("DATA (FORWARD)");
                #endif
		
                // Type of the last message sent
                output_msg_type = DATA;
	
                mesh_send(&zoundtracker_conn, &sink_addr);
	
                // Net Control Information
                num_msg_sended++;
				
                updateReadFile(&fmanNet);
            }

            // We're not pending for an 'ACK' message. Net work 
            // finished.
            state = BLOCKED;
            
            #ifdef DEBUG_STATE
              debug_state_current_state("BLOCKED");
            #endif
        }
    }
    else 
    {
        #ifdef DEBUG_NET
          debug_net_incorrect_checksum();
        #endif
    }
}

static void
broadcast_sent(struct broadcast_conn* c,int status, int num_tx)
{
    // To be completed..
}

static void 
broadcast_received(struct broadcast_conn* c,const rimeaddr_t *from)
{
    // Function: The behaviour of this function is very similar to the 
    // "received" "mesh" callback, to attend only the "HELLO_BS" 
    // messages sent through the broadcast connection. This function 
    // replies the "HELLO_BS" messages sending a "HELLO_MN" message to 
    // the "Basestation". If there's already a data sending, the 
    // "HELLO_BS" message is discarded. Else if "HELLO_BS" is the last 
    // message received, the new "HELLO_BS" message isn't discarded.

    // Context: This function sends a "HELLO_MN" message through the 
    // "mesh" connection when a "broadcast" "HELLO_BS" message received 
    // from the "Basestation".
    
    #ifdef DEBUG_NET
      debug_net_message_received_connection("broadcast");
    #endif
    
    if (state != DATA_SEND)
    {    
        // Getting the packet and checking checksum.
        packet_received = unmount_packet(packetbuf_dataptr());
        packet_checksum = compute_checksum(&packet_received);
        
        if (packet_checksum == packet_received.checksum)
        {
            if (packet_received.type == HELLO_BS)
            {
                #ifdef DEBUG_NET
                  debug_net_message_received("HELLO_BS");
                #endif
                
                // New Broadcast burst
                if (packet_received.data[0] != last_broadcast_id)
                {  
                    flooding_attempts = 0;
                        
                    // Control of 'HELLO_BS' messages
                    last_broadcast_id = packet_received.data[0];                
                    
                    #ifdef DEBUG_NET
                      debug_net_flooding_hello_bs();
                    #endif
                }
                
                if (flooding_attempts < MAX_FLOODING_ATTEMPTS)
                {
                    // Sending "HELLO_BS" through broadcast connection
                    mount_packet(&packet_received, rime_stream);
                    packetbuf_copyfrom((void *)rime_stream, PACKET_SIZE);
                    broadcast_send(&zoundtracker_broadcast_conn);                              
                    
                    flooding_attempts++;
                
                    #ifdef DEBUG_NET
                      debug_net_flooding_hello_bs();
                    #endif
                }
            }
            else
            {
                #ifdef DEBUG_NET
                  debug_net_message_received("incorrect");
                #endif
            }
        }
        else 
        {
            // Incorrect checksum. Invalid message. 
            #ifdef DEBUG_NET
              debug_net_incorrect_checksum();
            #endif
        }
    }
    else 
    {
        #ifdef DEBUG_NET
          debug_net_message_received("discarded");
        #endif
    }
}

const static struct mesh_callbacks 
  zoundtracker_callbacks = {received, sent, timedout};
const static struct broadcast_callbacks 
  zoundtracker_broadcast_callbacks = {broadcast_received, broadcast_sent};

//--------------------------------------------------------------------

// Sensor
void 
get_sensor_sample(void)
{
    // Function: This function takes a sample of the ZoundTracker 
    // board. Then stores the pair (number of sample, sample value) 
    // into the Filesystem. By this way we can associate every sample 
    // to every minute on the sample period.

    // Context:  This function is executed when the node enters into 
    // "DATA_COLLECT" state to take a sensor sample.
     	
    if (write_attempts == 0) 
    {
        int32_t t_buffer = 0;
        uint32_t sumbufhold = 0;
        uint16_t i = 0;

        for (i=0;i<255;i++) 
        {
            t_buffer = ztalog.value(PHIDGET3V_2);
            t_buffer -= sampref;
            sumbufhold += (uint32_t)(t_buffer * t_buffer);
        }
        
        sumbufhold = sumbufhold >> 8;

        #ifdef DEBUG_SENSOR
          printf("[Sensor]\n  Sum square avg: %lu\n", sumbufhold);     
        #endif

        // Building the 'Sample' structure for writing.
        current_sample.number = sample_interval;
        current_sample.value[0] = (char)((sumbufhold & 0xFF00)>>8);
        current_sample.value[1] = (char)(sumbufhold & 0x00FF);
    }

    write_bytes = write(&fmanLocal, &current_sample,SAMPLE_SIZE);
    if (write_bytes == ERROR_INVALID_FD) 
    {
        ++write_attempts;
        if (write_attempts < MAX_WRITE_ATTEMPTS) get_sensor_sample();
    }
    write_attempts = 0;
}

//--------------------------------------------------------------------

// Process
PROCESS(example_zoundt_mote_process, "Example zoundt mote process");
AUTOSTART_PROCESSES(&example_zoundt_mote_process);

PROCESS_THREAD(example_zoundt_mote_process, ev, data) {   
    
    PROCESS_EXITHANDLER(mesh_close(&zoundtracker_conn);)
    PROCESS_BEGIN();
    
    // NET
    rimeaddr_t my_addr;
    my_addr.u8[0] = MY_ADDR1;
    my_addr.u8[1] = MY_ADDR2;
    rimeaddr_set_node_addr(&my_addr);
    mesh_open(&zoundtracker_conn, CHANNEL1, &zoundtracker_callbacks);                                             
    broadcast_open(&zoundtracker_broadcast_conn, CHANNEL2, 
      &zoundtracker_broadcast_callbacks);       
    output_msg_type = EMPTY;
    ack_waiting = FALSE;
    attempts = 0;
    valid_broadcast_id = (unsigned char)(rand() % 256);
    num_msg_sended = 0;
    num_msg_acked = 0;
    packet_number = 1;
	
    // CFS
    etimer_set(&control_timer, NUM_SECONDS_SAMPLE*CLOCK_SECOND);
    sample_interval = 0;
    initOkLocal = FALSE;
    initOkNet = FALSE;
    initOkLocal = initFileManager(&fmanLocal, 'l', 10, 60);
    initOkNet = initFileManager(&fmanNet, 'n', 1, 60);
    read_attempts = 0;
    write_attempts = 0;
    
    // Sensor
    SENSORS_ACTIVATE(ztalog);

    // State
    state = BLOCKED;
    
    #ifdef DEBUG_FILEMAN
      debug_filesys_initOk(&fmanLocal,initOkLocal);
      debug_filesys_initOk(&fmanNet,initOkNet);
    #endif

    #ifdef DEBUG_STATE
      debug_state_current_state("BLOCKED");
    #endif

    while(sampcont < SAMPLE_NUMBER) {
      etimer_set(&samp_timer, SAMPLE_TIMER/80);
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&samp_timer));
      sampcont++;
      holdbuf += ztalog.value(PHIDGET3V_2);
    }

    sampref = holdbuf/SAMPLE_NUMBER;
    #ifdef DEBUG_SENSOR
      printf("[Sensor]\n");
      printf("  ADC Avg [%u]\n", sampref);
    #endif
	
    // Sending message ("HELLO_MN"). Changing to "DATA_SEND" from 
    // "BLOCKED" state.  
    state = DATA_SEND;    
    
    #ifdef DEBUG_STATE
      debug_state_current_state("DATA SEND");
    #endif
    
    // Sending first "HELLO_MN" message.
    hello_msg();
    
    while (TRUE)
    {
        PROCESS_WAIT_EVENT();
        if (ev == PROCESS_EVENT_TIMER)
        {
            #ifdef DEBUG_EVENT
              debug_event_current_event("Timer expired");
              printf("state = %d\n", state);
            #endif
            
            if (state == BLOCKED)
            {
                // Timer expired. Changing to "DATA_COLLECT" from 
                // "BLOCKED" state.
                state = DATA_COLLECT;
                
                #ifdef DEBUG_STATE
                  debug_state_current_state("DATA COLLECT");
                #endif
                      
                // 'sample_interval' starts on zero.
                get_sensor_sample();
                
                #ifdef DEBUG_SENSOR
                  debug_sensor_sample_measured(sample_interval);
                #endif
                
                // Updating state.
                sample_interval++;
            
                if (sample_interval == SEND_INTERVAL)
                {    
                    #ifdef DEBUG_NET
                      debug_net_info_net(num_msg_sended, num_msg_acked);
                    #endif
                
                    // Sensor samples collected. Changing to 
                    // "DATA_SEND" from "DATA_COLLECT" state.
                    state = DATA_SEND;
                    
                    #ifdef DEBUG_STATE
                      debug_state_current_state("DATA SEND");	
                    #endif
                    
                    send_packet_from_file();
                    sample_interval = 0;                
                }
                else
                {
                    // Collecting sensor samples. Changing 
                    // to "BLOCKED" from "DATA_COLLECT" state.
                    state = BLOCKED;
                    
                    #ifdef DEBUG_STATE
                      debug_state_current_state("BLOCKED");
                    #endif
                }
            }
            else if (state == DATA_SEND)
            {
                if (ack_waiting == TRUE)
                {
                    if (output_msg_type == DATA)
                    {
                        #ifdef DEBUG_NET
                          debug_net_message_lost(DATA_ACK,0);
                        #endif
                        
                        // ACK message lost.      
                        ack_waiting = FALSE;
     
                        // Starting a new sample period
                        sample_interval = 0;
                        readSeek(&fmanLocal, START_POSITION);
                        
                        // Returning to "BLOCKED" from "DATA_SEND" 
                        // state.
                        state = BLOCKED;
    					
                        #ifdef DEBUG_STATE
                          debug_state_current_state("BLOCKED");
                        #endif
                    }
                    else if (output_msg_type == HELLO_MN)
                    {
                        #ifdef DEBUG_NET
                          debug_net_message_lost(HELLO_ACK,0);
                        #endif
    					
                        ack_waiting = FALSE;
                        
                        // Returning to "BLOCKED" from "DATA_SEND" 
                        // state.
                        state = BLOCKED;
    					
                        #ifdef DEBUG_STATE
                          debug_state_current_state("BLOCKED");
                        #endif  
     
                    }
                    else if(output_msg_type == DATA_ACK)
                    {
                        #ifdef DEBUG_STATE
                          debug_state_current_state("DATA_SEND");
                          debug_net_output_message_type("DATA_ACK");
                        #endif
                    }
                    else if(output_msg_type == HELLO_ACK)
                    {
                        #ifdef DEBUG_STATE
                          debug_state_current_state("DATA_SEND");
                          debug_net_output_message_type("HELLO_ACK");
                        #endif
                    }                    
                }
            }       
        }
        else
        {	
            #ifdef DEBUG_EVENT
              debug_event_current_event("Unknown event");  
            #endif
        }
        
        // Reseting the timer. 
        etimer_set(&control_timer, NUM_SECONDS_SAMPLE*CLOCK_SECOND);
    }

    PROCESS_END(); 
}
