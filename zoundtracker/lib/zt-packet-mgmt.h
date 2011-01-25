#define SINK_ADDR1
#define SINK_ADDR2
#define HELLO_BS
#define HELLO_MN
#define DATA
#define ACK
#define POLL


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
unsigned char * mount_packet(Packet * my_packet);
Packet unmount_packet(unsigned char* my_array);

