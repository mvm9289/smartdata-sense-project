
#include "zt-net-lib.h"

unsigned int push(BYTE *packet_data)
{
    unsigned int result = 1;

    memcpy(&recv_queue[recv_indx_in*PACKET_SIZE], packet_data, PACKET_SIZE);

    recv_indx_in++;
    if (recv_indx_in >= RECV_QUEUE_SIZE) recv_indx_in = 0;

    recv_size++;
    if (recv_size > RECV_QUEUE_SIZE)
    {
        recv_size = RECV_QUEUE_SIZE;
        result = 0;
    }

    return result;
}

unsigned int pop(BYTE *packet_data)
{
    unsigned int result = 1;

    if (recv_size > 0)
    {
        memcpy(packet_data, &recv_queue[recv_indx_out*PACKET_SIZE], PACKET_SIZE);

        recv_indx_out++;
        if (recv_indx_out >= RECV_QUEUE_SIZE) recv_indx_out = 0;

        recv_size--;
    }
    else result = 0;

    return result;
}

Packet create_packet(unsigned int type)
{
    Packet packet;
    packet.addr1 = MY_ADDR1;
    packet.addr2 = MY_ADDR2;
    packet.type = type;
    if (type == HELLO_BS)
    {
        packet.size = 1;
        packet.data[0] = (unsigned char)(rand()%256);
    }
    else packet.size = 0;
    packet.counter = 0;
    packet.checksum = compute_checksum(&packet);

    return packet;
}

static void broadcast_received(    struct broadcast_conn *c,
                                   const rimeaddr_t *from    )
{
    if (packetbuf_datalen() == PACKET_SIZE)
    {
        #ifdef DEBUG_NET
            printf("             Broadcast received callback: ");
            printf("Packet received\n\n");
        #endif

        push((BYTE*)packetbuf_dataptr());
    }

    #ifdef DEBUG_NET
        else
        {
            printf("             Broadcast received callback: ");
            printf("Error with received packet size\n\n");
        }
    #endif
}

static void broadcast_sent(    struct broadcast_conn *ptr,
                               int status,
                               int num_tx    )
{
    #ifdef DEBUG_NET
        printf("             Broadcast sent callback: Packet sent\n\n");
    #endif
}

static void mesh_received(    struct mesh_conn *c,
                              const rimeaddr_t *from,
                              uint8_t hops    )
{
    if (packetbuf_datalen() == PACKET_SIZE)
    {
        #ifdef DEBUG_NET
            printf("             Mesh received callback: ");
            printf("Packet received\n\n");
        #endif

        push((BYTE*)packetbuf_dataptr());
    }

    #ifdef DEBUG_NET
        else
        {
            printf("             Mesh received callback: ");
            printf("Error with received packet size\n\n");
        }
    #endif
}

static void mesh_sent(    struct mesh_conn *c    )
{
    #ifdef DEBUG_NET
        printf("             Mesh sent callback: Packet sent\n\n");
    #endif
}

static void mesh_timedout(    struct mesh_conn *c    )
{
    #ifdef DEBUG_NET
        printf("             Mesh timedout callback: Packet timeout\n\n");
    #endif
}

void initNet()
{
    // Set rime address
    rimeaddr_t my_addr;
    my_addr.u8[0] = MY_ADDR1;
    my_addr.u8[1] = MY_ADDR2;
    rimeaddr_set_node_addr(&my_addr);
 
    // Open the mesh connection
    mesh_open(  &zt_mesh_conn,
                CHANNEL1,
                &mesh_callback);

    // Open the broadcast connection
    broadcast_open(   &zt_broadcast_conn,
                      CHANNEL2,
                      &broadcast_callback);
}

void stopNet()
{
    mesh_close(&zt_mesh_conn);
    broadcast_close(&zt_broadcast_conn);
}

void send(
    unsigned char addr1,
    unsigned char addr2,
    unsigned int packet_type,
    BYTE *packet_data,
    unsigned int data_size,
    unsigned char broadcast)
{
    rimeaddr_t addr_send;
    BYTE packet_buff[PACKET_SIZE];
    Packet packet;

    packet = create_packet(packet_type);
    
    if (packet_type == DATA || packet_type == DATA_FORWARD)
    {
        if (data_size > DATA_SIZE) data_size = DATA_SIZE;
        memcpy(packet.data, packet_data, data_size);
    }

    mount_packet(&packet, packet_buff);
    packetbuf_copyfrom((void *)packet_buff, PACKET_SIZE);
    
    if (broadcast)
        broadcast_send(&zt_broadcast_conn);
    else
    {
        addr_send.u8[0] = addr1;
        addr_send.u8[1] = addr2;
        mesh_send(&zoundtracker_conn, addr_send);
    }
}

unsigned int receive(Packet *packet)
{
    BYTE packet_buff[PACKET_SIZE];
    unsigned int result = 1;

    result = pop(packet_buff);

    if (result) *packet = unmount_packet(packet_buff);

    return result;
}

