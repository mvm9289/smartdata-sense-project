#include "zt-debug-lib.h"
#include "zt-packet-mgmt.h"
#include "string.h"

//NET messages
void debug_net_sending_message(char* message){
	printf("[net]\nSending '%s' message\n\n",message);
}

void debug_net_packet_content(Packet *packet){
	  int i;
	  printf("[net]\n 'DATA' packet contents\n");
	  printf("  Addr1: %d\n", packet->addr1);
	  printf("  Addr2: %d\n", packet->addr2);
	  printf("  Type: %d\n", packet->type);
	  printf("  Size: %d\n", packet->size);
	  printf("  Counter: %d\n", packet->counter);
	  printf("  Data: ");
	  for (i = 0; i < DATA_SIZE &&
                  packet->counter + i < packet->size;
                  i++)
                    printf("%d ", packet->data[i]);
	  printf("  Checksum: %d\n", packet->checksum);
      printf("  Battery: %d-%d\n\n", packet->reserved[20], packet->reserved[21]);
}


void debug_net_sending_WORKING_FILE(int packet_number){
	printf("[net]\n Sending the 'WORKING_FILE'");
	printf(" (packet number: %d)\n\n", packet_number);
}


void debug_net_timeout(int type,int packet_number){
	if(type == HELLO_MN)
		printf("[net]\n Timeout resending 'HELLO_MN' message\n\n");
	else if(type == DATA) {
	    printf("[net]\n Timeout resending the current packet");
        printf(" (packet number: %d)", packet_number);
        printf(" from the 'WORKING_FILE'\n\n");		
	}
}

void debug_net_max_attempts(){
	printf("[net]\n Maximum number of attempts reached\n");
}


void debug_net_message_lost(int type, int packet_number){
	if(type == HELLO_MN)
		printf("[net]\n 'HELLO_MN' message lost\n\n");
	else if(type == DATA){
		printf("[net]\n 'DATA' message lost");
		printf(" (packet number: %d)\n\n", packet_number);
	}
    else if(type == DATA_ACK)
        printf("[net]\n 'DATA_ACK' message lost\n\n");
   else if(type == HELLO_ACK)
        printf("[net]\n 'DATA_ACK' message lost\n\n");

   else
        printf("[net]\n Unknown type message lost\n\n");
}


void debug_net_message_received_connection(char* connection){
	printf("[net]\n Message received through '%s' connection\n\n",connection);
}


void debug_net_message_received(char* message){
	if(strcmp(message,"incorrect") == 0)
		printf("[net]\n Incorrect type of message received\n\n");
	else printf("[net]\n '%s' received\n\n",message);
}


void debug_net_incorrect_checksum(){
	printf("[net]\n Incorrect checksum invalid message\n\n");
}


void debug_net_info_net(int packets_sent, int packets_acknowledged){
	printf("[net]\n");
    printf(" Number of packets sent: %d\n", packets_sent);
	printf(" Number of packets acknowledged: %d\n\n", packets_acknowledged);
}


//STATE messages
void debug_state_current_state(char *state){
	printf("---\n[state]\n Current state '%s'\n---\n\n",state);
}


//FILEMAN messages
//TODO

//EVENT messages
void debug_event_timer_expired(){
	printf("[event]\n Timer expired event\n\n");
}

//SENSOR messages
void debug_sensor_sample_measured(int sample_number){
	printf("[sensor]\n Sample measured");
	printf(" (sample number: %d)\n\n", sample_number);
}


