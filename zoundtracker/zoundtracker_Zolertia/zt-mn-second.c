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
#include "lib/zt-packet-mgmt.h"
#include "dev/i2cmaster.h"
// #include "dev/zt.h"
#include "dev/battery-sensor.h"

// Provisional
#include "dev/z1-phidgets.h"

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
#define SAMPLE_SIZE 4

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
static unsigned char original_addr1, original_addr2;


// Sensor
static int sample_interval;

// State
static unsigned char state;
static unsigned char previous_state;

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
data_msg(int type) 
{   
    // Function: This function builds and sends a "DATA" message to the 
    // "Basestation". In this message there's a set of sensor samples 
    // that the node saved in a file on the Filesystem.

    // Context: This message is sent to the "Basestation" periodically 
    // every NUM_SECONDS_SAMPLE*SEND_INTERVAL seconds. However the 
    // "DATA" message is sent to reply a "POLL" message sent by the 
    // "Basestation".

    // Disable other ADC channel, because was getting in the way
    SENSORS_DEACTIVATE(phidgets);

    Packet packet_to_send;
     
    // "Packet" construction
    if(type == DATA)
    {
      packet_to_send.addr1 = MY_ADDR1;
      packet_to_send.addr2 = MY_ADDR2;
    }
    else if(type == DATA_FORWARD)
    {
      packet_to_send.addr1 = original_addr1;
      packet_to_send.addr2 = original_addr2;
    }
    packet_to_send.type = type;
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
    
    #ifdef DEBUG_NET
      debug_net_battery(batt);
    #endif

    SENSORS_DEACTIVATE(battery_sensor);
    SENSORS_ACTIVATE(phidgets);

    packet_to_send.reserved[20] = (unsigned char) ((batt & 0xFF00) >> 8);
    packet_to_send.reserved[21] = (unsigned char) (batt & 0x00FF);
    
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
      if(type == DATA)
        debug_net_sending_message("DATA");
      else if(type == DATA_FORWARD)
        debug_net_sending_message("DATA FORWARD");      
    #endif
    
    // Type of the last message sent
      if(type == DATA)
        output_msg_type = DATA;
      else if(type == DATA_FORWARD)
        output_msg_type = DATA_FORWARD;	

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
prepare_packet(int type)
{
    // Function: This function reads one file from the Filesystem to 
    // get a set of sensor samples for the next packet to send.

    // Context: This function is called from the function 
    // 'send_packet_from_file' to check if there's more sensor samples 
    // to send in the Filesystem.

    // Return: The return value will be TRUE if 'read_bytes' is 
    // greater than 0, otherwise the returns FALSE

    
    if (read_attempts > 0) 
    {
        int pos = -1;
        if(type == DATA)
            pos = readSeek(&fmanLocal, START_POSITION);
        else if(type == DATA_FORWARD)
            pos = readSeek(&fmanNet, START_POSITION);

        if(pos == ERROR_INVALID_FD) 
        {
            ++read_attempts;
            if(read_attempts < MAX_READ_ATTEMPTS)return prepare_packet(type);
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
    
    if(type == DATA) 
        read_bytes = read(&fmanLocal, read_buffer, DATA_SIZE);
    else if(type == DATA_FORWARD)
    {
        read_bytes = read(&fmanNet, &original_addr1, sizeof(unsigned char));
        read_bytes = read(&fmanNet, &original_addr2, sizeof(unsigned char));      
        read_bytes = read(&fmanNet, read_buffer, DATA_SIZE);
    }

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
        if(read_attempts < MAX_READ_ATTEMPTS)return prepare_packet(type);
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
send_packet_from_file(int type)
{
    // Function: This function checks if there's sensor samples to 
    // send in the Filesystem and if so, sends a "DATA" message.
    
    // Context: This function is called when we want to send the first 
    // packet from a file, and when a "DATA_ACK" or "POLL" message is 
    // received.

      int storedFiles;
      if(type == DATA)
         storedFiles = getStoredFiles(&fmanLocal);
      else if(type == DATA_FORWARD)
         storedFiles = getStoredFiles(&fmanNet);	

      next_packet = prepare_packet(type);
	      
      if (next_packet == TRUE)
      {              
          // Packet sending
          // packet_number++;
          data_msg(type);
	
          #ifdef DEBUG_NET
            debug_net_sending_WORKING_FILE(packet_number);
          #endif
      }    
      else if (next_packet == FALSE) 
      {       
          if(type == DATA) 
          {
              sample_interval = 0; //Se puede eliminar?
              updateReadFile(&fmanLocal);
              storedFiles = getStoredFiles(&fmanLocal);
          }
          else if(type == DATA_FORWARD)
          {
              updateReadFile(&fmanNet);
              storedFiles = getStoredFiles(&fmanNet);

          }
          if (storedFiles > 0) send_packet_from_file(type);
          else if(type == DATA_FORWARD) 
          {
            printf("No hay paquetes del Net Filesystem por enviar\n\n");
            previous_state = state;
            state = BLOCKED;
          }
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
    if(output_msg_type == DATA || output_msg_type == HELLO_MN || 
        output_msg_type == DATA_FORWARD)
    	    ack_waiting = TRUE;
    else if (output_msg_type == DATA_ACK || output_msg_type == DATA_FORWARD_ACK)
    {
      unsigned char aux_state = previous_state;
      previous_state = state;
      state = aux_state;
    }

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
            data_msg(DATA);

            #ifdef DEBUG_NET
              debug_net_timeout(DATA, packet_number);
            #endif
        }
        else if (output_msg_type == DATA_FORWARD)
        {
            // Resending "DATA_FORWARD" message.
            data_msg(DATA_FORWARD);

            #ifdef DEBUG_NET
              debug_net_timeout(DATA_FORWARD, packet_number);
            #endif
        }
        else if(output_msg_type == DATA_ACK || output_msg_type == DATA_FORWARD_ACK)
        {
          // Resending buffer info (ACK)
           mesh_send(&zoundtracker_conn, from);
           #ifdef DEBUG_NET
              debug_net_timeout(DATA_FORWARD, packet_number);
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
        else if (output_msg_type == DATA_FORWARD)
        {	  
            readSeek(&fmanNet, START_POSITION);
        
            #ifdef DEBUG_NET
              debug_net_message_lost(DATA_FORWARD,packet_number);
            #endif
        }
                
        // Message lost. Changing to "BLOCKED" from "DATA_SEND" state.  
        previous_state = state;
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

    //unsigned char prev_state = state;
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
        previous_state = state;
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
            send_packet_from_file(DATA);        
        }
        else if (packet_received.type == DATA_FORWARD_ACK)
        {	
            ack_waiting = FALSE;

            // Net Control Information
            num_msg_acked++;

            #ifdef DEBUG_NET
              debug_net_message_received("DATA_FORWARD_ACK");
            #endif

            // Trying to send the next packet           
            send_packet_from_file(DATA_FORWARD);        
        }

        else if (packet_received.type == POLL)
        {
            #ifdef DEBUG_NET
              debug_net_message_received("POLL");
            #endif

            if (previous_state == DATA_SEND)
            {
                // "POLL" message on reply to a "DATA" message sent 
                // and lost. Resending the last packet.
                data_msg(output_msg_type);
            }
            else
            {
                // "POLL" message advertisement. Node timestamp exceed. 
                // Sending the stored files.
                send_packet_from_file(DATA);
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
        else if(packet_received.type == DATA || packet_received.type == DATA_FORWARD)
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

            // Saving the packet into the net filesystem (original address and data)
            i = write(&fmanNet, &packet_received.addr1,sizeof(unsigned char));
            i = write(&fmanNet, &packet_received.addr2,sizeof(unsigned char));
            i = write(&fmanNet, packet_received.data,DATA_SIZE);
            printf("%d bytes written\n",i);

            if(i == DATA_SIZE) 
            {
                // Mounting ACK message
                if(packet_received.type == DATA) 
                    ACK_msg(DATA_ACK);
                else 
                    ACK_msg(DATA_FORWARD_ACK);

                // Preparing "Packet" to send it through "rime". Building 
                // the "rime_stream" using the information of "ack_packet"   
                mount_packet(&ack_packet, rime_stream);
                packetbuf_copyfrom((void *)rime_stream, PACKET_SIZE);
	
                #ifdef DEBUG_NET
                  if(packet_received.type == DATA) 
                    debug_net_sending_message("DATA ACK");
                  else
                    debug_net_sending_message("DATA FORWARD ACK");
                #endif


                // Type of the last message sent
                if(packet_received.type == DATA) 
                    output_msg_type = DATA_ACK;
                else
	                output_msg_type = DATA_FORWARD_ACK;

                mesh_send(&zoundtracker_conn, from);

              

                // Net Control Information
                num_msg_sended++;

            }
                

        }
        else 
        {
            #ifdef DEBUG_NET
              debug_net_message_received("incorrect");
            #endif
        }
        
        if(packet_received.type == DATA_ACK)
            storedFiles = getStoredFiles(&fmanLocal);
        else if(packet_received.type == DATA_FORWARD_ACK)
            storedFiles = getStoredFiles(&fmanNet);
        
        if (ack_waiting == FALSE && storedFiles==0)
        {
            // We're not pending for an 'ACK' message. Net work 
            // finished.
            previous_state = state;
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
        uint32_t bigassbuffer = 0;
        uint8_t i = 0;

        for (i=0;i<200;i++) 
        {
            t_buffer = phidgets.value(PHIDGET3V_2);
            t_buffer -= 2031;
            bigassbuffer += (uint32_t)(t_buffer * t_buffer);
        }
        
        bigassbuffer /= 200;
        printf("[ZT]\n Raw mean square: %lu\n", bigassbuffer);     
        bigassbuffer = (bigassbuffer >> 1);

        // Building the 'Sample' structure for writing.
        current_sample.number = sample_interval;
        current_sample.value[0] = (char)((bigassbuffer & 0xFF0000)>>16);
        current_sample.value[1] = (char)((bigassbuffer & 0x00FF00)>>8);
        current_sample.value[2] = (char)(bigassbuffer & 0x0000FF);
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
    initOkNet = initFileManager(&fmanNet, 'n', 3, 60);
    read_attempts = 0;
    write_attempts = 0;
    
    // Sensor
    //zt_init();
    SENSORS_ACTIVATE(phidgets);

    // State
    state = BLOCKED;
    
    #ifdef DEBUG_FILEMAN
      debug_filesys_initOk(&fmanLocal,initOkLocal);
      debug_filesys_initOk(&fmanNet,initOkNet);
    #endif

    #ifdef DEBUG_STATE
      debug_state_current_state("BLOCKED");
    #endif
	
    // Sending message ("HELLO_MN"). Changing to "DATA_SEND" from 
    // "BLOCKED" state.  
    previous_state = state;
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
                previous_state = state;
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
                    //Sending local files (fmanLocal)
                    #ifdef DEBUG_NET
                      debug_net_info_net(num_msg_sended, num_msg_acked);
                    #endif
                
                    // Sensor samples collected. Changing to 
                    // "DATA_SEND" from "DATA_COLLECT" state.
                    previous_state = state;
                    state = DATA_SEND;
                    
                    #ifdef DEBUG_STATE
                      debug_state_current_state("DATA SEND");	
                    #endif
                    
                    send_packet_from_file(DATA);
                    sample_interval = 0;                
                }
                else if(sample_interval == SEND_INTERVAL/2)
                {
                    //Sending net files (fmanNet)
                    #ifdef DEBUG_NET
                      debug_net_info_net(num_msg_sended, num_msg_acked);
                    #endif
                
                    // Sensor samples collected. Changing to 
                    // "DATA_SEND" from "DATA_COLLECT" state.
                    previous_state = state;
                    state = DATA_SEND;
                    
                    #ifdef DEBUG_STATE
                      debug_state_current_state("DATA FORWARD SEND");	
                    #endif
                    
                    send_packet_from_file(DATA_FORWARD);

                    
                }
                else
                {
                    // Collecting sensor samples. Changing 
                    // to "BLOCKED" from "DATA_COLLECT" state.
                    previous_state = state;
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
                        previous_state = state;
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
                        previous_state = state;
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
