#ifndef __ZT_PACKET_MGMT_H__
#define __ZT_PACKET_MGMT_H__

#ifndef SINK_ADDR1
    #define SINK_ADDR1 111
#endif
#ifndef SINK_ADDR2
    #define SINK_ADDR2 111
#endif

#define CHANNEL1 111
#define CHANNEL2 222
#define HELLO_BS 1
#define HELLO_MN 2
#define DATA 3
#define HELLO_ACK 4
#define DATA_ACK 5
#define POLL 6
#define DATA_FORWARD 7
#define DATA_FORWARD_ACK 8
#define PACKET_SIZE 64
#define DATA_SIZE 40
#define HEADER_SIZE 7
#define TAIL_SIZE 2
#define RESERVED_SIZE (PACKET_SIZE - HEADER_SIZE - DATA_SIZE - TAIL_SIZE)
#define HELLO_MSG_SIZE 0
#define HELLO_MSG_COUNTER 0

typedef struct {
  unsigned char addr1;
  unsigned char addr2;
  unsigned char type;
  unsigned short size;
  unsigned short counter;  // Data stream offset
  unsigned char data[DATA_SIZE];
  unsigned char reserved[RESERVED_SIZE];
  unsigned short checksum;
}Packet;

typedef struct {
  unsigned char number;
  #ifdef ZT
    char value[2]; 
  #else
    char value[3];
  #endif
}Sample;

typedef struct {
  unsigned char number;
  char minus;
  int tempint;
  unsigned int tempfrac;  
}TempSample;


unsigned short compute_checksum(Packet* my_packet);
// PRE: packet must be an array of PACKET_SIZE bytes
void mount_packet(Packet * my_packet, unsigned char* packet) ;
Packet unmount_packet(unsigned char* my_array);

#endif /*__ZT_PACKET_MGMT_H__*/
