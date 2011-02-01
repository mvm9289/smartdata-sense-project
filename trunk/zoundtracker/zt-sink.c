
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
#define STATE_TRANSITION_PERIOD CLOCK_SECOND/SECOND_PARTITION
#define MAX_RESENDS 5
#define DATA_TIMEOUT 60*12

// States
#define HELLO_STATE 1
#define WAIT_STATE 2
#define UPDATE_STATE 3
#define TEST_STATE 4
#define POLL_STATE 5
///////////////////////////////////////////////////////////////////////

PROCESS(zoundtracker_sink_process, "ZoundTracker Sink Process");
AUTOSTART_PROCESSES(&zoundtracker_sink_process);

///////////////////////////////////////////////////////////////////////
///////////////////////// Static global Vars //////////////////////////
typedef struct
{
    unsigned int timestamp;
    unsigned int resends;
    unsigned char addr1;
    unsigned char addr2;
    unsigned char empty;
} NodeTableEntry;

static NodeTableEntry nodeTable[NODE_TABLE_SIZE];

static struct mesh_conn zoundtracker_conn;
static struct trickle_conn zt_broadcast_conn;

static unsigned int state;

static unsigned short partial_seconds;
static unsigned short seconds;
///////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////
////////////////////////////// Functions //////////////////////////////
Packet hello_packet()
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

Packet hello_ack_packet()
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

Packet data_ack_packet()
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

Packet poll_packet()
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
static void broadcast_received(struct trickle_conn *c)
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
        if (state == POLL_STATE)
            printf("Mesh timedout callback: Poll packet timeout\n\n");
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
    int i, first_empty_entry;
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
                printf("Computed checksum dont match with packet checksum\n\n");
                printf("Mesh reveived callback: Sending POLL packet\n\n");
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
                        printf("Mesh received callback: Hello packet received from %d.%d\n\n", from->u8[0], from->u8[1]);
                    #endif
                    is_valid = 1;
                    recv_send_packet = hello_ack_packet();
                    break;
                case DATA:
                    #ifdef DEBUG_NET
                        printf("Mesh received callback: Data packet received from %d.%d\n\n", from->u8[0], from->u8[1]);
                        printf("Mesh received callback: Packet content:\n");
                        printf("MOBILE NODE ID: %d.%d\n", recv_packet.addr1, recv_packet.addr2);
                        printf("MESSAGE TYPE: %d\n", recv_packet.type);
                        printf("SIZE: %d\n", recv_packet.size);
                        printf("COUNTER: %d\n", recv_packet.counter);
                        printf("DATA: ");
                        for (i = 0; i < DATA_SIZE && recv_packet.counter + i < recv_packet.size; i++)
                            printf("%d ", recv_packet.data[i]);
                        printf("\n");
                        printf("CHECKSUM: %d\n\n", recv_packet.checksum);
                    #else
                        for (i = 0; i < PACKET_SIZE; i++) putchar(((unsigned char*)packetbuf_dataptr())[i]);
                    #endif
                    is_valid = 1;
                    recv_send_packet = data_ack_packet();
                    break;
                default:
                    #ifdef DEBUG_NET
                        printf("Mesh received callback: Unknown message type (%c)\n\n", recv_packet.type);
                    #endif
                    break;
            }
            if (is_valid)
            {
                first_empty_entry = -1;
                found = 0;
                for (i = 0; i < NODE_TABLE_SIZE && !found; i++)
                {
                    if (nodeTable[i].empty && first_empty_entry == -1)
                        first_empty_entry = i;
                    else if (!nodeTable[i].empty &&
                      nodeTable[i].addr1 == from->u8[0] &&
                      nodeTable[i].addr2 == from->u8[1])
                        found = 1;
                }
                if (found)
                {
                    nodeTable[i].timestamp = 0;
                    nodeTable[i].resends = 0;
                }
                else if (first_empty_entry != -1)
                {
                    nodeTable[first_empty_entry].empty = 0;
                    nodeTable[first_empty_entry].timestamp = 0;
                    nodeTable[first_empty_entry].resends = 0;
                    nodeTable[first_empty_entry].addr1 = from->u8[0];
                    nodeTable[first_empty_entry].addr2 = from->u8[1];
                }
                #ifdef DEBUG_NET
                    else printf("Mesh received callback: Node table is full\n\n");
                    printf("Mesh reveived callback: Sending ACK packet\n\n");
                #endif
                mount_packet(&recv_send_packet, recv_packet_buff);
                packetbuf_copyfrom((void *)recv_packet_buff, PACKET_SIZE);
                mesh_send(&zoundtracker_conn, from);
            }
        }
    }
    #ifdef DEBUG_NET
        else printf("Mesh received callback: Error with received packet size\n\n");
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
PROCESS_THREAD(zoundtracker_sink_process, ev, data)
{
    PROCESS_EXITHANDLER(mesh_close(&zoundtracker_conn);)
    PROCESS_BEGIN();
    
    ///////////////////////////////////////////////////////////////////
    /////////////////////////////// Vars //////////////////////////////
    int i;
    rimeaddr_t my_addr;
    rimeaddr_t addr_send;
    static struct etimer timer;
    unsigned char packet_buff[PACKET_SIZE];
    Packet packet;
    ///////////////////////////////////////////////////////////////////
    
    ///////////////////////////////////////////////////////////////////
    ///////////////////////// Initialization //////////////////////////
    // Node table
    for (i = 0; i < NODE_TABLE_SIZE; i++)
        nodeTable[i].empty = 1;
    // Sink rime address
    my_addr.u8[0] = SINK_ADDR1;
    my_addr.u8[1] = SINK_ADDR2;
    rimeaddr_set_node_addr(&my_addr);
    // Mesh connection
    mesh_open(  &zoundtracker_conn,
                CHANNEL1,
                &zoundtracker_sink_callbacks);
    // Trickle connection
    //~ trickle_open(   &zt_broadcast_conn,
                    //~ 0,
                    //~ CHANNEL2,
                    //~ &broadcast_callback);
    // Timer
    partial_seconds = 0;
    seconds = 0;
    // State
    state = HELLO_STATE;
    ///////////////////////////////////////////////////////////////////

    etimer_set(&timer, STATE_TRANSITION_PERIOD);
    while (1)
    {
        PROCESS_WAIT_EVENT();
        
        if (etimer_expired(&timer))
        {
            partial_seconds++;
            switch (state)
            {
                case HELLO_STATE:
                    #if defined(DEBUG_STATES) || defined(DEBUG_NET)
                        printf("HELLO_STATE: Sending hello packet\n\n");
                    #endif
                    packet = hello_packet();
                    mount_packet(&packet, packet_buff);
                    packetbuf_copyfrom((void *)packet_buff, PACKET_SIZE);
                    trickle_open(   &zt_broadcast_conn,
                                    0,
                                    CHANNEL2,
                                    &broadcast_callback);
                    trickle_send(&zt_broadcast_conn);
                    trickle_close(&zt_broadcast_conn);
                    #ifdef DEBUG_NET
                        printf("HELLO_STATE: Hello packet sent\n\n");
                    #endif
                    //~ if (partial_seconds >= SECOND_PARTITION)
                    //~ {
                        //~ seconds++; // seconds += partial_seconds/SECOND_PARTITION; ???????????????????
                        //~ partial_seconds = 0; // partial_seconds = partial_seconds%SECOND_PARTITION; ??????????????
                        //~ state = UPDATE_STATE;
                    //~ }
                    //~ else state = TEST_STATE;
                    state = UPDATE_STATE;
                    break;
                case WAIT_STATE:
                    #ifdef DEBUG_STATES
                        printf("WAIT_STATE: Waiting for anything happens\n\n");
                    #endif
                    if (partial_seconds >= SECOND_PARTITION)
                    {
                        seconds++; // seconds += partial_seconds/SECOND_PARTITION; ???????????????????
                        partial_seconds = 0; // partial_seconds = partial_seconds%SECOND_PARTITION; ??????????????
                        if (seconds >= 3600)
                        {
                            seconds = 0;
                            state = HELLO_STATE;
                        }
                        else state = UPDATE_STATE;
                    }
                    break;
                case UPDATE_STATE:
                    #ifdef DEBUG_STATES
                        printf("UPDATE_STATE: Updating timestamp for all Node table\n\n");
                    #endif
                    for (i = 0; i < NODE_TABLE_SIZE; i++)
                        if (!nodeTable[i].empty) nodeTable[i].timestamp++;
                    state = TEST_STATE;
                    break;
                case TEST_STATE:
                    #ifdef DEBUG_STATES
                        printf("TEST_STATE: Testing if some node need to be polled\n\n");
                    #endif
                    for (i = 0; i < NODE_TABLE_SIZE && state == TEST_STATE; i++)
                        if (!nodeTable[i].empty && nodeTable[i].timestamp >= DATA_TIMEOUT)
                            state = POLL_STATE;
                    if (state == TEST_STATE) state = WAIT_STATE;
                    break;
                case POLL_STATE:
                    #ifdef DEBUG_STATES
                        printf("POLL_STATE: Polling all nodes that has been timedout\n\n");
                    #endif
                    packet = poll_packet();
                    mount_packet(&packet, packet_buff);
                    //packetbuf_copyfrom((void *)packet_buff, PACKET_SIZE); //?????????????????????
                    for (i = 0; i < NODE_TABLE_SIZE; i++)
                        if (!nodeTable[i].empty && nodeTable[i].timestamp >= DATA_TIMEOUT)
                        {
                            if (nodeTable[i].resends >= MAX_RESENDS)
                            {
                                nodeTable[i].empty = 1;
                                #ifdef DEBUG_NET
                                    printf("POLL_STATE: MAX_RESENDS have been reached from node %d.%d\n\n", nodeTable[i].addr1, nodeTable[i].addr2);
                                #endif
                            }
                            else
                            {
                                #ifdef DEBUG_NET
                                    printf("POLL_STATE: Sending poll packet to node %d.%d\n\n", nodeTable[i].addr1, nodeTable[i].addr2);
                                #endif
                                nodeTable[i].resends++;
                                addr_send.u8[0] = nodeTable[i].addr1;
                                addr_send.u8[1] = nodeTable[i].addr2;
                                packetbuf_copyfrom((void *)packet_buff, PACKET_SIZE); //?????????????????????
                                mesh_send(&zoundtracker_conn, &addr_send);
                            }
                        }
                    if (partial_seconds >= SECOND_PARTITION)
                    {
                        seconds++; // seconds += partial_seconds/SECOND_PARTITION; ???????????????????
                        partial_seconds = 0; // partial_seconds = partial_seconds%SECOND_PARTITION; ??????????????
                        if (seconds >= 3600)
                        {
                            seconds = 0;
                            state = HELLO_STATE;
                        }
                        else state = UPDATE_STATE;
                    }
                    else state = TEST_STATE;
                    break;
                default:
                    break;
            }
            etimer_set(&timer, STATE_TRANSITION_PERIOD);
        }
    }
    PROCESS_END();
}
///////////////////////////////////////////////////////////////////////
