#define SINK_ADDR1 111
#define SINK_ADDR2 111
#define CHANNEL 111
#define HELLO_BS 1
#define HELLO_MN 2
#define DATA 3
#define HELLO_ACK 4
#define DATA_ACK 5
#define POLL 6
#define PACKET_SIZE 64
#define DATA_SIZE 32
#define HELLO_MSG_SIZE 5

typedef struct {
  unsigned char addr1;
  unsigned char addr2;
  unsigned char type;
  unsigned short size;
  unsigned short counter;
  unsigned char data[32];
  unsigned char reserved[23];
  unsigned short checksum;
}Packet;

unsigned short compute_checksum(Packet* my_packet);
// PRE: packet must be an array of 64 Bytes
void mount_packet(Packet * my_packet, unsigned char* packet) ;
Packet unmount_packet(unsigned char* my_array);

