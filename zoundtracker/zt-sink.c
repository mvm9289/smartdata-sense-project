
#include "contiki.h"
#include "net/rime.h"
#include "net/rime/mesh.h"

#include <stdio.h>
#include <string.h>

#include "lib/zt-packet-mgmt.h"

// Configuration
#define NODE_TABLE_SIZE 128
#define STATE_TRANSITION_PERIOD CLOCK_SECOND/10
#define MAX_RESENDS 5
#define DATA_TIMEOUT CLOCK_SECOND*60*12

// States
#define HELLO_STATE 1
#define WAIT_STATE 2
#define UPDATE_STATE 3
#define TEST_STATE 4
#define POLL_STATE 5



PROCESS(zoundtracker_sink_process, "ZoundTracker Sink Process");
AUTOSTART_PROCESSES(&zoundtracker_sink_process);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////// Static global Vars //////////////////////////////////////////////////////
typedef struct                                                                                                                 //
{                                                                                                                              //
    unsigned int timestamp;                                                                                                    //
    unsigned int resends;                                                                                                      //
    unsigned char addr1;                                                                                                       //
    unsigned char addr2;                                                                                                       //
    unsigned char empty;                                                                                                       //
} NodeTableEntry;                                                                                                              //
//                                                                                                                             //
static NodeTableEntry nodeTable[NODE_TABLE_SIZE];                                                                              //
//                                                                                                                             //
static struct mesh_conn zoundtracker_conn;                                                                                     //
//                                                                                                                             //
static unsigned int state;                                                                                                     //
//                                                                                                                             //
static unsigned short partial_seconds;                                                                                         //
static unsigned short seconds;                                                                                                 //
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////// Mesh connection callbacks ///////////////////////////////////////////////////
static void sent(struct mesh_conn *c)
{
    printf("Packet sent.\n\n");
}

static void timedout(struct mesh_conn *c)
{
    printf("Packet timeouted. Resends = %d\n\n", resends+1);
    if (resends++ < MAX_RESENDS)
    {
        rimeaddr_t addr_send;
        addr_send.u8[0] = nodeTable[i].addr1;
        addr_send.u8[1] = nodeTable[i].addr2;
        packetbuf_copyfrom("poll", 4);
        
        mesh_send(&zoundtracker_conn, &addr_send);
    }
    else isReceived = 1;
}

static void received(struct mesh_conn *c, const rimeaddr_t *from, uint8_t hops)
{
    if (!memcmp(packetbuf_dataptr(), (void *)"hello", 5))
    {
        leds_on(LEDS_GREEN);
        printf("Hello packet received from %d.%d\n\n", from->u8[0], from->u8[1]);
        int j, firstEmptyPos = -1;
        unsigned char exit = 0;
        for (j = 0; j < NODE_TABLE_SIZE && !exit; j++)
        {
            if (nodeTable[j].empty) firstEmptyPos = j;
            else if (nodeTable[j].addr1 == from->u8[0] &&
              nodeTable[j].addr2 == from->u8[1])
                exit = 1;
        }
        if (firstEmptyPos == -1) printf("NODE TABLE COMPLETE!\n\n");
        else if (exit) printf("This node already exists in node table\n\n");
        else
        {
            nodeTable[firstEmptyPos].empty = 0;
            nodeTable[firstEmptyPos].addr1 = from->u8[0];
            nodeTable[firstEmptyPos].addr2 = from->u8[1];
        }
    }
    else
    {
        leds_on(LEDS_RED);
        leds_off(LEDS_YELLOW);
        printf("Packet received:\n From: %d.%d\n Data length: %d\n Data: %.*s\n Hops: %d\n\n",
            from->u8[0], from->u8[1], packetbuf_datalen(),
            packetbuf_datalen(), (char *)packetbuf_dataptr(), hops);
        isReceived = 1;
    }
}

const static struct mesh_callbacks zoundtracker_sink_callbacks = {received, sent, timedout};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////// Functions ///////////////////////////////////////////////////////////
Packet hello_packet()                                                                                                           //
{                                                                                                                              //
    Packet helloBS;                                                                                                            //
    helloBS.addr1 = SINK_ADDR1;                                                                                                //
    helloBS.addr2 = SINK_ADDR2;                                                                                                //
    helloBS.type = HELLO_BS;                                                                                                   //
    helloBS.size = 0;                                                                                                          //
    helloBS.counter = 0;                                                                                                       //
    helloBS.checksum = compute_checksum(&helloBS);                                                                             //
                                                                                                                               //
    return helloBS;                                                                                                            //
}                                                                                                                              //
//                                                                                                                             //
Packet poll_packet()                                                                                                            //
{                                                                                                                              //
    Packet poll;                                                                                                               //
    poll.addr1 = SINK_ADDR1;                                                                                                   //
    poll.addr2 = SINK_ADDR2;                                                                                                   //
    poll.type = POLL;                                                                                                          //
    poll.size = 0;                                                                                                             //
    poll.counter = 0;                                                                                                          //
}                                                                                                                              //
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////// Main ///////////////////////////////////////////////////////////////
PROCESS_THREAD(zoundtracker_sink_process, ev, data)
{
    PROCESS_EXITHANDLER(mesh_close(&zoundtracker_conn);)
    PROCESS_BEGIN();
    
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////// Initialization //////////////////////////////////////////////////////
    // Node table                                                                                                         //
    int j;                                                                                                                //
    for (j = 0; j < NODE_TABLE_SIZE; j++) nodeTable[j].empty = 1;                                                         //
    //                                                                                                                    //
    // Sink rime address                                                                                                  //
    rimeaddr_t my_addr;                                                                                                   //
    my_addr.u8[0] = SINK_ADDR1;                                                                                           //
    my_addr.u8[1] = SINK_ADDR2;                                                                                           //
    rimeaddr_set_node_addr(&my_addr);                                                                                     //
    //                                                                                                                    //
    // Mesh connection                                                                                                    //
    mesh_open(&zoundtracker_conn, CHANNEL, &zoundtracker_sink_callbacks);                                                 //
    //                                                                                                                    //
    // Timer                                                                                                              //
    static struct etimer timer;                                                                                           //
    partial_seconds = 0;                                                                                                  //
    seconds = 0;                                                                                                          //
    // State                                                                                                              //
    state = HELLO_STATE;                                                                                                  //
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    
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
                    Packet hello = hello_packet();
                    
                    break;
                case WAIT_STATE:
                    break;
                case UPDATE_STATE:
                    break;
                case TEST_STATE:
                    break;
                case POLL_STATE:
                    break;
                default:
                    break;
            }
            etimer_set(&timer, STATE_TRANSITION_PERIOD);
        }
    }
    
    while (1)
    {
        //PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));
        while(!etimer_expired(&timer));
        for (i = 0; i < NODE_TABLE_SIZE; i++)
        {
            if (!nodeTable[i].empty)
            {
                rimeaddr_t addr_send;
                addr_send.u8[0] = nodeTable[i].addr1;
                addr_send.u8[1] = nodeTable[i].addr2;
                packetbuf_copyfrom((void *)"poll", 4);
                
                isReceived = 0;
                resends = 0;
                leds_off(LEDS_GREEN);
                leds_off(LEDS_RED);
                leds_on(LEDS_YELLOW);
                printf("Sending poll packet\n\n");
                mesh_send(&zoundtracker_conn, &addr_send);
                
                static struct etimer timer2;
                etimer_set(&timer2, CLOCK_SECOND*2);
                while (!isReceived && resends < MAX_RESENDS)
                {
                    //PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer2));
                    while (!etimer_expired(&timer2));
                    resends++;
                    leds_off(LEDS_GREEN);
                    leds_off(LEDS_RED);
                    leds_on(LEDS_YELLOW);
                    printf("Sending poll packet\n\n");
                    mesh_send(&zoundtracker_conn, &addr_send);
                    etimer_reset(&timer2);
                }
                if (!isReceived)
                {
                    printf("Answer packet not received. Entry %d of node table will be erased.\n\n", i);
                    nodeTable[i].empty = 1;
                }
            }
        }
        etimer_reset(&timer);
    }
    PROCESS_END();
}
