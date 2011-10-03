#ifndef __ZT_PACKET_MGMT_H__
#define __ZT_PACKET_MGMT_H__


#define SINK_MACID 0xff01
#define HELLO_BS 1
#define HELLO_MN 2
#define DATA 3
#define HELLO_ACK 4
#define DATA_ACK 5
#define POLL 6
#define DATA_FORWARD 7
#define DATA_FORWARD_ACK 8
#define PACKET_SIZE 64
#define DATA_SIZE 32
#define HELLO_MSG_SIZE 0
#define HELLO_MSG_COUNTER 0

typedef struct {
  unsigned short macid;
  unsigned char type;
  unsigned short size;
  unsigned short counter;  // Data stream offset
  unsigned char data[32];
  unsigned char reserved[23];
  unsigned short checksum;
}Packet;

typedef struct {
  unsigned char number;
  char value; 
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
