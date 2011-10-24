// TODO:
//    -Check the checksum and resend last packet
//    -Add RSSI to the packets ??
//    -Other reserved data ??
//    -Control of the ack waiting here ??

#ifndef __ZT_NET_LIB_H__
#define __ZT_NET_LIB_H__

/**********************************************/
/**************** Net includes ****************/
/**********************************************/

#include <stdlib.h>
#include <string.h>


/* Include each net protocol used in this module */
#include "net/rime.h"
#include "net/rime/mesh.h"
#include "net/rime/broadcast.h"

#include "zt-packet-mgmt.h"


/**********************************************/
/******************* Defines ******************/
/**********************************************/

#define RECV_QUEUE_SIZE 5


/**********************************************/
/************** Useful typedefs ***************/
/**********************************************/

typedef unsigned char BYTE;


/**********************************************/
/************* Static global vars *************/
/**********************************************/

static struct mesh_conn zt_mesh_conn;
static struct broadcast_conn zt_broadcast_conn;

static BYTE recv_queue[PACKET_SIZE*RECV_QUEUE_SIZE];
static int recv_indx_in = 0;
static int recv_indx_out = 0;
static int recv_size = 0;


/**********************************************/
/********* Received queue management **********/
/**********************************************/

inline unsigned int push(BYTE *packet_data);
inline unsigned int pop(BYTE *packet_data);


/**********************************************/
/************ Packet constructor *************/
/**********************************************/

inline Packet create_packet(unsigned int type);


/**********************************************/
/*************** Net callbacks ****************/
/**********************************************/

static void broadcast_received(    struct broadcast_conn *c,
                                   const rimeaddr_t *from    );
static void broadcast_sent(    struct broadcast_conn *ptr,
                               int status,
                               int num_tx    );
const static struct broadcast_callbacks broadcast_callback =
{
    broadcast_received,
    broadcast_sent
};

static void mesh_received(    struct mesh_conn *c,
                              const rimeaddr_t *from,
                              uint8_t hops    );
static void mesh_sent(    struct mesh_conn *c    );
static void mesh_timedout(    struct mesh_conn *c    );
const static struct mesh_callbacks mesh_callback =
{
    mesh_received,
    mesh_sent,
    mesh_timedout
};


/**********************************************/
/************* Module functions ***************/
/**********************************************/

void initNet();
void stopNet();
void send(
    unsigned char addr1,
    unsigned char addr2,
    unsigned int packet_type,
    BYTE *packet_data,
    unsigned int data_size,
    unsigned char broadcast);
unsigned int receive(Packet *packet);

#endif
