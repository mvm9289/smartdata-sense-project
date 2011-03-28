
#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"
#include "dev/cc2420.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib/zt-packet-mgmt.h"

///////////////////////////////////////////////////////////////////////
/////////////////////////////// Defines ///////////////////////////////
// Configuration
#define NODE_TABLE_SIZE 32
#define SECOND_PARTITION 10
#define STATE_TRANSITION_PERIOD CLOCK_SECOND/SECOND_PARTITION // 1 ds
#define MAX_RESENDS 5
//#define DATA_TIMEOUT 60*12 // 12 min
#define DATA_TIMEOUT 115
#define POLL_PERIOD 5*SECOND_PARTITION // 5 s
//#define HELLO_PERIOD 60*60 // 1 hour
#define HELLO_PERIOD 60*10 // 1 hour
#define HELLO_SENDS 1

#define UDP_IP_BUF   ((struct uip_udpip_hdr *)&uip_buf[UIP_LLH_LEN])

#ifdef DEBUG_MODE
#define DEBUG_STATES
#define DEBUG_NET
#endif

// States
#define HELLO_STATE 1
#define WAIT_STATE 2
#define UPDATE_STATE 3
#define TEST_STATE 4
#define POLL_STATE 5
///////////////////////////////////////////////////////////////////////

PROCESS(    zoundtracker_sink_process,
            "ZoundTracker Sink Process"    );
AUTOSTART_PROCESSES(    &zoundtracker_sink_process    );

///////////////////////////////////////////////////////////////////////
///////////////////////// Static global Vars //////////////////////////
typedef struct
{
    unsigned int timestamp;
    unsigned int resends;
    unsigned int poll_wait;
    unsigned short macid;
    unsigned char empty;
} Node_table_entry;

static Node_table_entry node_table[NODE_TABLE_SIZE];

static struct uip_udp_conn *server_conn;
///////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////
////////////////////////////// Functions //////////////////////////////
inline Packet hello_packet()
{
    Packet helloBS;
    helloBS.macid = SINK_MACID;
    helloBS.type = HELLO_BS;
    helloBS.size = 1;
    helloBS.counter = 0;
    helloBS.data[0] = (unsigned char)(rand() % 256);
    helloBS.checksum = compute_checksum(&helloBS);
    
    return helloBS;
}

inline Packet hello_ack_packet()
{
    Packet helloACK;
    helloACK.macid = SINK_MACID;
    helloACK.type = HELLO_ACK;
    helloACK.size = 0;
    helloACK.counter = 0;
    helloACK.checksum = compute_checksum(&helloACK);
    
    return helloACK;
}

inline Packet data_ack_packet()
{
    Packet dataACK;
    dataACK.macid = SINK_MACID;
    dataACK.type = DATA_ACK;
    dataACK.size = 0;
    dataACK.counter = 0;
    dataACK.checksum = compute_checksum(&dataACK);
    
    return dataACK;
}

inline Packet poll_packet()
{
    Packet poll;
    poll.macid = SINK_MACID;
    poll.type = POLL;
    poll.size = 0;
    poll.counter = 0;
    poll.checksum = compute_checksum(&poll);
    
    return poll;
}
///////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////
////////////////////////////// IPv6 HANDLER ///////////////////////////
static void tcpip_handler(void)
{
    Packet recv_packet;
    unsigned short recv_checksum;
    Packet recv_send_packet;
    unsigned char recv_packet_buff[PACKET_SIZE];
    int i, first_empty_entry, found_node, rssi;
    unsigned char found;
    unsigned char is_valid;
        
        
    if(uip_newdata())
    {
        if (uip_datalen() >= PACKET_SIZE)
        {
            recv_packet = unmount_packet(
                (unsigned char*)uip_appdata);
            recv_checksum = compute_checksum(&recv_packet);
            if (recv_checksum != recv_packet.checksum)
            {
                #ifdef DEBUG_NET
                    printf("             TCPIP Handler: ");
                    printf("Computed checksum dont match with packet ");
                    printf("checksum\n\n");
                    printf("             TCPIP Handler: Sending POLL ");
                    printf("packet\n\n");
                #endif
                recv_send_packet = poll_packet();
                /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
                uip_ipaddr_copy(&server_conn->ripaddr, &UDP_IP_BUF->srcipaddr);
                server_conn->rport = UDP_IP_BUF->srcport;
                mount_packet(&recv_send_packet, recv_packet_buff);
                uip_udp_packet_send(server_conn, recv_packet_buff, PACKET_SIZE);
                /* Restore server connection to allow data from any node */
                memset(&server_conn->ripaddr, 0, sizeof(server_conn->ripaddr));
                server_conn->rport = 0;
            }
            else
            {
                is_valid = 0;
                if (recv_packet.type == HELLO_MN)
                {
                    #ifdef DEBUG_NET
                        printf("             TCPIP Handler: ");
                        printf("Hello packet received from %x\n\n",
                                recv_packet.macid);
                    #endif
                    is_valid = 1;
                    recv_send_packet = hello_ack_packet();
                    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
                    uip_ipaddr_copy(&server_conn->ripaddr, &UDP_IP_BUF->srcipaddr);
                    server_conn->rport = UDP_IP_BUF->srcport;
                }
                else if (recv_packet.type == DATA)
                {
                    rssi = cc2420_rssi();
                    recv_packet.reserved[3] = (char)(rssi>>8);
                    recv_packet.reserved[4] = (char)(rssi);
                    #ifdef DEBUG_NET
                        printf("             TCPIP Handler: ");
                        printf("Data packet received from %x\n\n",
                                recv_packet.macid);
                        printf("             TCPIP Handler: ");
                        printf("Packet content:\n");
                        printf("             MOBILE NODE ID: %x\n",
                                recv_packet.macid);
                        printf("             MESSAGE TYPE: %d\n", recv_packet.type);
                        printf("             SIZE: %d\n", recv_packet.size);
                        printf("             COUNTER: %d\n", recv_packet.counter);
                        printf("             DATA: ");
                        for (i = 0; i < DATA_SIZE &&
                          recv_packet.counter + i < recv_packet.size;
                          i++)
                            printf("%d ", recv_packet.data[i]);
                        printf("             \n");
                        printf("             RSSI: %d\n", rssi);
                        printf("             NUMBER OF PACKETS SENDED: %d\n",
                                recv_packet.reserved[0]);
                        printf("             NUMBER OF PACKETS ACKNOWLEDGED: %d\n",
                                recv_packet.reserved[1]);
                        printf("             NUMBER OF FILES SENDED: %d\n",
                                recv_packet.reserved[2]);
                        printf("             CHECKSUM: %d\n\n",
                                recv_packet.checksum);
                    #else
                        mount_packet(&recv_packet, recv_packet_buff);
                        for (i = 0; i < PACKET_SIZE; i++)
                            putchar(recv_packet_buff[i]);
                    #endif
                    is_valid = 1;
                    recv_send_packet = data_ack_packet();
                    //////////////////////////////////////////////////////////////////////////////////////////////////////////
                    uip_ipaddr_copy(&server_conn->ripaddr, &UDP_IP_BUF->srcipaddr);
                    server_conn->rport = UDP_IP_BUF->srcport;
                }
                else
                {
                    #ifdef DEBUG_NET
                        printf("             TCPIP Handler: ");
                        printf( "Unknown message type (%c)\n\n",
                                recv_packet.type);
                    #endif
                }
                if (is_valid)
                {
                    first_empty_entry = -1;
                    found = 0;
                    found_node = -1;
                    for (i = 0; i < NODE_TABLE_SIZE && !found; i++)
                    {
                        if (node_table[i].empty && first_empty_entry == -1)
                            first_empty_entry = i;
                        else if (!node_table[i].empty &&
                          node_table[i].macid == recv_packet.macid)
                        {
                            found = 1;
                            found_node = i;
                        }
                    }
                    if (found)
                    {
                        node_table[found_node].timestamp = 0;
                        node_table[found_node].resends = 0;
                        node_table[found_node].poll_wait = 0;
                    }
                    else if (first_empty_entry != -1)
                    {
                        node_table[first_empty_entry].empty = 0;
                        node_table[first_empty_entry].timestamp = 0;
                        node_table[first_empty_entry].resends = 0;
                        node_table[first_empty_entry].poll_wait = 0;
                        node_table[first_empty_entry].macid = recv_packet.macid;
                    }
                    #ifdef DEBUG_NET
                        else
                        {
                            printf("             TCPIP Handler: ");
                            printf("Node table is full\n\n");
                        }
                        printf("             TCPIP Handler: ");
                        printf("Sending ACK packet\n\n");
                    #endif
                    mount_packet(&recv_send_packet, recv_packet_buff);
                    uip_udp_packet_send(server_conn, recv_packet_buff, PACKET_SIZE);
                    /* Restore server connection to allow data from any node */
                    memset(&server_conn->ripaddr, 0, sizeof(server_conn->ripaddr));
                    server_conn->rport = 0;
                }
            }
        }
        #ifdef DEBUG_NET
            else
            {
                printf("             TCPIP Handler: ");
                printf("Error with received packet size\n\n");
            }
        #endif
    }
}

static void set_connection_address(uip_ipaddr_t *ipaddr, unsigned short macid)
{
  // change this IP address depending on the node that runs the server!
  uip_ip6addr(ipaddr,0xfe80,0,0,0,0xc30c,0,0,macid);
}

///////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////
///////////////////////////////// Main ////////////////////////////////
PROCESS_THREAD(    zoundtracker_sink_process, ev, data    )
{
    PROCESS_BEGIN();
    
    ///////////////////////////////////////////////////////////////////
    /////////////////////////////// Vars //////////////////////////////
    int i;
    unsigned char packet_buff[PACKET_SIZE];
    uip_ipaddr_t ipaddr;
    static Packet packet;
    static unsigned int hello_sends;
    static struct etimer timer;
    static unsigned short partial_seconds;
    static unsigned short seconds;
    static unsigned int state;
    ///////////////////////////////////////////////////////////////////
    
    ///////////////////////////////////////////////////////////////////
    ///////////////////////// Initialization //////////////////////////
    // Clear node table
    for (i = 0; i < NODE_TABLE_SIZE; i++)
        node_table[i].empty = 1;
    // Open the UDP IPv6 connection
    server_conn = udp_new(NULL, 0, NULL);
    udp_bind(server_conn, 3000);
    // Clear timer data
    partial_seconds = 0;
    seconds = 0;
    // Set first state
    hello_sends = 0;
    state = HELLO_STATE;
    ///////////////////////////////////////////////////////////////////

    etimer_set(&timer, STATE_TRANSITION_PERIOD);
    while (1)
    {
        PROCESS_WAIT_EVENT();
        
        if(ev == tcpip_event) tcpip_handler();
            
        if (etimer_expired(&timer))
        {
            partial_seconds++;
            ///////////////////////////////////////////////////////////
            ///////////////////// Hello state /////////////////////////
            if (state == HELLO_STATE)
            {
                #if defined(DEBUG_STATES) || defined(DEBUG_NET)
                    printf("             HELLO_STATE: ");
                    printf("Sending hello packet\n\n");
                #endif
                if (hello_sends == 0) packet = hello_packet();
                mount_packet(&packet, packet_buff);
                ////////////////////////////////////////////////////////// TO IPV6!!!!!!!!!!!!!!!!!!!!///////////////////////////////
                //~ packetbuf_copyfrom( (void *)packet_buff,
                                    //~ PACKET_SIZE);
                //~ broadcast_open(   &zt_broadcast_conn,
                                  //~ CHANNEL2,
                                  //~ &broadcast_callback);
                //~ broadcast_send(&zt_broadcast_conn);
                //~ broadcast_close(&zt_broadcast_conn);
                #ifdef DEBUG_NET
                    printf("             HELLO_STATE: Hello packet sent\n\n");
                #endif
                hello_sends++;
                state = UPDATE_STATE;
            }
            ///////////////////////////////////////////////////////////
            
            ///////////////////////////////////////////////////////////
            ///////////////////// Wait state //////////////////////////
            else if (state == WAIT_STATE)
            {
                #ifdef DEBUG_STATES
                    printf("             WAIT_STATE: ");
                    printf("Waiting for anything happens\n\n");
                #endif
                if (partial_seconds >= SECOND_PARTITION)
                {
                    seconds++;
                    partial_seconds = 0;
                    // <----------------------------------------------------------------------------------------------------------
                    //~ seconds += partial_seconds/SECOND_PARTITION;
                    //~ partial_seconds =   partial_seconds%
                                        //~ SECOND_PARTITION;
                    if (seconds >= HELLO_PERIOD)
                    {
                        seconds = 0;
                        hello_sends = 0;
                        state = HELLO_STATE;
                    }
                    else if (hello_sends < HELLO_SENDS) state = HELLO_STATE;
                    else state = UPDATE_STATE;
                }
            }
            ///////////////////////////////////////////////////////////
            
            ///////////////////////////////////////////////////////////
            //////////////////// Update state /////////////////////////
            else if (state == UPDATE_STATE)
            {
                #ifdef DEBUG_STATES
                    printf("             UPDATE_STATE: ");
                    printf("Updating timestamp for all nodes\n\n");
                #endif
                for (i = 0; i < NODE_TABLE_SIZE; i++)
                    if (!node_table[i].empty)
                        node_table[i].timestamp++;
                state = TEST_STATE;
            }
            ///////////////////////////////////////////////////////////
            
            ///////////////////////////////////////////////////////////
            ////////////////////// Test state /////////////////////////
            else if (state == TEST_STATE)
            {
                #ifdef DEBUG_STATES
                    printf("             TEST_STATE: ");
                    printf("Testing if some node need to be ");
                    printf("polled\n\n");
                #endif
                for (i = 0; i < NODE_TABLE_SIZE &&
                  state == TEST_STATE; i++)
                    if (!node_table[i].empty &&
                      node_table[i].timestamp >= DATA_TIMEOUT)
                        state = POLL_STATE;
                if (state == TEST_STATE) state = WAIT_STATE;
            }
            ///////////////////////////////////////////////////////////
            
            ///////////////////////////////////////////////////////////
            ////////////////////// Poll state /////////////////////////
            else if (state == POLL_STATE)
            {
                #ifdef DEBUG_STATES
                    printf("             POLL_STATE: ");
                    printf("Polling all nodes that has been ");
                    printf("timedout\n\n");
                #endif
                packet = poll_packet();
                mount_packet(&packet, packet_buff);
                // <--------------------------------------------------------------------------------------------------------------
                //~ packetbuf_copyfrom( (void *)packet_buff,
                                    //~ PACKET_SIZE);
                for (i = 0; i < NODE_TABLE_SIZE; i++)
                    if (!node_table[i].empty &&
                      node_table[i].timestamp >= DATA_TIMEOUT)
                    {
                        if (node_table[i].resends >= MAX_RESENDS)
                        {
                            node_table[i].empty = 1;
                            #ifdef DEBUG_NET
                                printf("             POLL_STATE: ");
                                printf("MAX_RESENDS have been ");
                                printf("reached from node ");
                                printf("%x\n\n", node_table[i].macid);
                            #endif
                        }
                        else if (node_table[i].poll_wait == 0)
                        {
                            #ifdef DEBUG_NET
                                printf("             POLL_STATE: ");
                                printf("Sending poll packet to ");
                                printf("node %x\n\n", node_table[i].macid);
                            #endif
                            node_table[i].resends++;
                            node_table[i].poll_wait = POLL_PERIOD;
                            ////////////////////////////////////////////////////////// TO IPV6!!!!!!!!!!!!!!!!!!!!///////////////////////////////
                            //~ addr_send.u8[0] = node_table[i].addr1;
                            //~ addr_send.u8[1] = node_table[i].addr2;
                            //~ // <--------------------------------------------------------------------------------------------------
                            //~ packetbuf_copyfrom(
                                //~ (void *)packet_buff,
                                //~ PACKET_SIZE);
                            //~ mesh_send(  &zoundtracker_conn,
                                        //~ &addr_send);
                        }
                        else node_table[i].poll_wait--;
                    }
                if (partial_seconds >= SECOND_PARTITION)
                {
                    seconds++;
                    partial_seconds = 0;
                    // <----------------------------------------------------------------------------------------------------------
                    //~ seconds += partial_seconds/SECOND_PARTITION;
                    //~ partial_seconds =   partial_seconds%
                                        //~ SECOND_PARTITION;
                    if (seconds >= HELLO_PERIOD)
                    {
                        seconds = 0;
                        hello_sends = 0;
                        state = HELLO_STATE;
                    }
                    else if (hello_sends < HELLO_SENDS) state = HELLO_STATE;
                    else state = UPDATE_STATE;
                }
                else state = TEST_STATE;
            }
            ///////////////////////////////////////////////////////////
            
            else
            {
                #ifdef DEBUG_STATES
                    printf("             UNKNOWN STATE\n\n");
                #endif
            }
            etimer_set(&timer, STATE_TRANSITION_PERIOD);
        }
    }
    PROCESS_END();
}
///////////////////////////////////////////////////////////////////////
