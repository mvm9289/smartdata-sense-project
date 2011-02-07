
#include "contiki.h"
#include "net/rime.h"
#include "net/rime/mesh.h"
#include "net/rime/trickle.h"

#include <stdio.h>
#include <string.h>

#include "lib/zt-packet-mgmt.h"

///////////////////////////////////////////////////////////////////////
/////////////////////////////// Defines ///////////////////////////////
// Configuration
#define NODE_TABLE_SIZE 128
#define SECOND_PARTITION 10
#define STATE_TRANSITION_PERIOD CLOCK_SECOND/SECOND_PARTITION // 1 ds
#define MAX_RESENDS 5
#define DATA_TIMEOUT 60*12 // 12 min
#define POLL_PERIOD 5*SECOND_PARTITION // 5 s
#define HELLO_PERIOD 30 // 1 hour
#define INIT_WAIT 2*CLOCK_SECOND

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
    unsigned char addr1;
    unsigned char addr2;
    unsigned char empty;
} Node_table_entry;

static Node_table_entry node_table[NODE_TABLE_SIZE];

static struct mesh_conn zoundtracker_conn;
static struct trickle_conn zt_broadcast_conn;
///////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////
////////////////////////////// Functions //////////////////////////////
inline Packet hello_packet()
{
    Packet helloBS;
    helloBS.addr1 = SINK_ADDR1;
    helloBS.addr2 = SINK_ADDR2;
    helloBS.type = HELLO_BS;
    helloBS.size = 0;
    helloBS.counter = 0;
    helloBS.checksum = compute_checksum(&helloBS);
    
    return helloBS;
}

inline Packet hello_ack_packet()
{
    Packet helloACK;
    helloACK.addr1 = SINK_ADDR1;
    helloACK.addr2 = SINK_ADDR2;
    helloACK.type = HELLO_ACK;
    helloACK.size = 0;
    helloACK.counter = 0;
    helloACK.checksum = compute_checksum(&helloACK);
    
    return helloACK;
}

inline Packet data_ack_packet()
{
    Packet dataACK;
    dataACK.addr1 = SINK_ADDR1;
    dataACK.addr2 = SINK_ADDR2;
    dataACK.type = DATA_ACK;
    dataACK.size = 0;
    dataACK.counter = 0;
    dataACK.checksum = compute_checksum(&dataACK);
    
    return dataACK;
}

inline Packet poll_packet()
{
    Packet poll;
    poll.addr1 = SINK_ADDR1;
    poll.addr2 = SINK_ADDR2;
    poll.type = POLL;
    poll.size = 0;
    poll.counter = 0;
    poll.checksum = compute_checksum(&poll);
    
    return poll;
}
///////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////
///////////////////// Trickle connection callback /////////////////////
static void broadcast_received(    struct trickle_conn *c    )
{
    #ifdef DEBUG_NET
        printf("Trickle received callback: ");
        printf("Broadcast packet received\n\n");
    #endif
}

const static struct trickle_callbacks broadcast_callback =
{
    broadcast_received
};
///////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////
////////////////////// Mesh connection callbacks //////////////////////
static void sent(    struct mesh_conn *c    )
{
    #ifdef DEBUG_NET
        printf("Mesh sent callback: Packet sent\n\n");
    #endif
}

static void timedout(    struct mesh_conn *c    )
{
    #ifdef DEBUG_NET
        printf("Mesh timedout callback: Packet timeout\n\n");
    #endif
}

static void received(    struct mesh_conn *c,
                         const rimeaddr_t *from,
                         uint8_t hops    )
{
    Packet recv_packet;
    unsigned short recv_checksum;
    Packet recv_send_packet;
    unsigned char recv_packet_buff[PACKET_SIZE];
    int i, first_empty_entry, found_node;
    unsigned char found;
    unsigned char is_valid;
    
    if (packetbuf_datalen() >= PACKET_SIZE)
    {
        recv_packet = unmount_packet(
            (unsigned char*)packetbuf_dataptr());
        recv_checksum = compute_checksum(&recv_packet);
        if (recv_checksum != recv_packet.checksum)
        {
            #ifdef DEBUG_NET
                printf("Mesh received callback: ");
                printf("Computed checksum dont match with packet ");
                printf("checksum\n\n");
                printf("Mesh reveived callback: Sending POLL ");
                printf("packet\n\n");
            #endif
            recv_send_packet = poll_packet();
            mount_packet(&recv_send_packet, recv_packet_buff);
            packetbuf_copyfrom((void *)recv_packet_buff, PACKET_SIZE);
            mesh_send(&zoundtracker_conn, from);
        }
        else
        {
            is_valid = 0;
            switch (recv_packet.type)
            {
                case HELLO_MN:
                    #ifdef DEBUG_NET
                        printf("Mesh received callback: ");
                        printf( "Hello packet received from %d.%d\n\n",
                                from->u8[0],
                                from->u8[1]);
                    #endif
                    is_valid = 1;
                    recv_send_packet = hello_ack_packet();
                    break;
                case DATA:
                    #ifdef DEBUG_NET
                        printf("Mesh received callback: ");
                        printf( "Data packet received from %d.%d\n\n",
                                from->u8[0],
                                from->u8[1]);
                        printf("Mesh received callback: ");
                        printf("Packet content:\n");
                        printf( "MOBILE NODE ID: %d.%d\n",
                                recv_packet.addr1,
                                recv_packet.addr2);
                        printf("MESSAGE TYPE: %d\n", recv_packet.type);
                        printf("SIZE: %d\n", recv_packet.size);
                        printf("COUNTER: %d\n", recv_packet.counter);
                        printf("DATA: ");
                        for (i = 0; i < DATA_SIZE &&
                          recv_packet.counter + i < recv_packet.size;
                          i++)
                            printf("%d ", recv_packet.data[i]);
                        printf("\n");
                        printf( "CHECKSUM: %d\n\n",
                                recv_packet.checksum);
                    #else
                        for (i = 0; i < PACKET_SIZE; i++)
                            putchar(((char*)packetbuf_dataptr())[i]);
                    #endif
                    is_valid = 1;
                    recv_send_packet = data_ack_packet();
                    break;
                default:
                    #ifdef DEBUG_NET
                        printf("Mesh received callback: ");
                        printf( "Unknown message type (%c)\n\n",
                                recv_packet.type);
                    #endif
                    break;
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
                      node_table[i].addr1 == from->u8[0] &&
                      node_table[i].addr2 == from->u8[1])
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
                    node_table[first_empty_entry].addr1 = from->u8[0];
                    node_table[first_empty_entry].addr2 = from->u8[1];
                }
                #ifdef DEBUG_NET
                    else
                    {
                        printf("Mesh received callback: ");
                        printf("Node table is full\n\n");
                    }
                    printf("Mesh reveived callback: ");
                    printf("Sending ACK packet\n\n");
                #endif
                mount_packet(&recv_send_packet, recv_packet_buff);
                packetbuf_copyfrom( (void *)recv_packet_buff,
                                    PACKET_SIZE);
                mesh_send(&zoundtracker_conn, from);
            }
        }
    }
    #ifdef DEBUG_NET
        else
        {
            printf("Mesh received callback: ");
            printf("Error with received packet size\n\n");
        }
    #endif
}

const static struct mesh_callbacks zoundtracker_sink_callbacks =
{
    received,
    sent,
    timedout
};
///////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////
///////////////////////////////// Main ////////////////////////////////
PROCESS_THREAD(    zoundtracker_sink_process, ev, data    )
{
    PROCESS_EXITHANDLER(mesh_close(&zoundtracker_conn);)
    PROCESS_BEGIN();
    
    ///////////////////////////////////////////////////////////////////
    /////////////////////////////// Vars //////////////////////////////
    int i;
    rimeaddr_t my_addr;
    rimeaddr_t addr_send;
    unsigned char packet_buff[PACKET_SIZE];
    Packet packet;
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
    // Set sink rime address
    my_addr.u8[0] = SINK_ADDR1;
    my_addr.u8[1] = SINK_ADDR2;
    rimeaddr_set_node_addr(&my_addr);
    // Open the mesh connection
    mesh_open(  &zoundtracker_conn,
                CHANNEL1,
                &zoundtracker_sink_callbacks);
    trickle_open(   &zt_broadcast_conn,
                    0,
                    CHANNEL2,
                    &broadcast_callback);
    // Clear timer data
    partial_seconds = 0;
    seconds = 0;
    // Set first state
    state = HELLO_STATE;
    ///////////////////////////////////////////////////////////////////
    
    etimer_set(&timer, INIT_WAIT);
    PROCESS_WAIT_EVENT();
    while(!etimer_expired(&timer)) PROCESS_WAIT_EVENT();

    etimer_set(&timer, STATE_TRANSITION_PERIOD);
    while (1)
    {
        PROCESS_WAIT_EVENT();
        
        if (etimer_expired(&timer))
        {
            partial_seconds++;
            switch (state)
            {
                ///////////////////////////////////////////////////////
                ///////////////////// Hello state /////////////////////
                case HELLO_STATE:
                    #if defined(DEBUG_STATES) || defined(DEBUG_NET)
                        printf("HELLO_STATE: ");
                        printf("Sending hello packet\n\n");
                    #endif
                    packet = hello_packet();
                    mount_packet(&packet, packet_buff);
                    packetbuf_copyfrom( (void *)packet_buff,
                                        PACKET_SIZE);
                    //~ trickle_open(   &zt_broadcast_conn,
                                    //~ 0,
                                    //~ CHANNEL2,
                                    //~ &broadcast_callback);
                    trickle_send(&zt_broadcast_conn);
                    //~ trickle_close(&zt_broadcast_conn);
                    #ifdef DEBUG_NET
                        printf("HELLO_STATE: Hello packet sent\n\n");
                    #endif
                    state = UPDATE_STATE;
                    break;
                ///////////////////////////////////////////////////////
                
                ///////////////////////////////////////////////////////
                ///////////////////// Wait state //////////////////////
                case WAIT_STATE:
                    #ifdef DEBUG_STATES
                        printf("WAIT_STATE: ");
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
                            state = HELLO_STATE;
                        }
                        else state = UPDATE_STATE;
                    }
                    break;
                ///////////////////////////////////////////////////////
                
                ///////////////////////////////////////////////////////
                //////////////////// Update state /////////////////////
                case UPDATE_STATE:
                    #ifdef DEBUG_STATES
                        printf("UPDATE_STATE: ");
                        printf("Updating timestamp for all nodes\n\n");
                    #endif
                    for (i = 0; i < NODE_TABLE_SIZE; i++)
                        if (!node_table[i].empty)
                            node_table[i].timestamp++;
                    state = TEST_STATE;
                    break;
                ///////////////////////////////////////////////////////
                
                ///////////////////////////////////////////////////////
                ////////////////////// Test state /////////////////////
                case TEST_STATE:
                    #ifdef DEBUG_STATES
                        printf("TEST_STATE: ");
                        printf("Testing if some node need to be ");
                        printf("polled\n\n");
                    #endif
                    for (i = 0; i < NODE_TABLE_SIZE &&
                      state == TEST_STATE; i++)
                        if (!node_table[i].empty &&
                          node_table[i].timestamp >= DATA_TIMEOUT)
                            state = POLL_STATE;
                    if (state == TEST_STATE) state = WAIT_STATE;
                    break;
                ///////////////////////////////////////////////////////
                
                ///////////////////////////////////////////////////////
                ////////////////////// Poll state /////////////////////
                case POLL_STATE:
                    #ifdef DEBUG_STATES
                        printf("POLL_STATE: ");
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
                                    printf("POLL_STATE: ");
                                    printf("MAX_RESENDS have been ");
                                    printf("reached from node ");
                                    printf( "%d.%d\n\n",
                                            node_table[i].addr1,
                                            node_table[i].addr2);
                                #endif
                            }
                            else if (node_table[i].poll_wait == 0)
                            {
                                #ifdef DEBUG_NET
                                    printf("POLL_STATE: ");
                                    printf("Sending poll packet to ");
                                    printf( "node %d.%d\n\n",
                                            node_table[i].addr1,
                                            node_table[i].addr2);
                                #endif
                                node_table[i].resends++;
                                node_table[i].poll_wait = POLL_PERIOD;
                                addr_send.u8[0] = node_table[i].addr1;
                                addr_send.u8[1] = node_table[i].addr2;
                                // <--------------------------------------------------------------------------------------------------
                                packetbuf_copyfrom(
                                    (void *)packet_buff,
                                    PACKET_SIZE);
                                mesh_send(  &zoundtracker_conn,
                                            &addr_send);
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
                            state = HELLO_STATE;
                        }
                        else state = UPDATE_STATE;
                    }
                    else state = TEST_STATE;
                    break;
                ///////////////////////////////////////////////////////
                    
                default:
                    #ifdef DEBUG_STATES
                        printf("UNKNOWN STATE\n\n");
                    #endif
                    break;
            }
            etimer_set(&timer, STATE_TRANSITION_PERIOD);
        }
    }
    PROCESS_END();
}
///////////////////////////////////////////////////////////////////////
