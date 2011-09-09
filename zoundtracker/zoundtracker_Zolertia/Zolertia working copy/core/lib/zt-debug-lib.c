#include "zt-debug-lib.h"
#include "zt-filesys-lib.h"

//NET messages
void debug_net_sending_message(char* message)
{
	printf("[net]\n Sending '%s' message\n\n",message);
}

void debug_net_sent_message()
{
	printf("[net]\n Sent message\n\n");
}

void debug_net_packet_content(Packet *packet)
{
	  int i;
          unsigned int temp = 0;
          u32_t tmp = (packet->reserved[18]<<8)+packet->reserved[19];
          tmp *= 5000;
          tmp /= 4096;

	  printf("[net]\n 'DATA' packet contents\n");
	  printf("  Addr1: %d\n", packet->addr1);
	  printf("  Addr2: %d\n", packet->addr2);
	  printf("  Type: %d\n", packet->type);
	  printf("  Size: %d\n", packet->size);
	  printf("  Counter: %d\n", packet->counter);
	  printf("  Checksum: %d\n", packet->checksum);
          printf("  Battery: %d\n", tmp);
	  printf("  Data:\n  ");

	  for (i=0; i < DATA_SIZE && packet->counter + i < packet->size; i++){
            printf("(%d) ", packet->data[i]);
            i++;            
            temp = (packet->data[i]<<8);
            i++;  
            temp += packet->data[i];
            printf("%u ", temp);
          }	  

          printf("\n");

}


void debug_net_sending_WORKING_FILE(int packet_number)
{
	printf("[net]\n Sending the 'WORKING_FILE'");
	printf(" (packet number: %d)\n\n", packet_number);
}


void debug_net_timeout(int type,int packet_number)
{
	if(type == HELLO_MN)
		printf("[net]\n Timeout resending 'HELLO_MN' message\n\n");
	else if(type == DATA)
	{
	    printf("[net]\n Timeout resending the current packet");
        printf(" (packet number: %d)", packet_number);
        printf(" from the 'WORKING_FILE'\n\n");		
	}
}

void debug_net_max_attempts()
{
	printf("[net]\n Maximum number of attempts reached\n");
}


void debug_net_message_lost(int type, int packet_number)
{
	if(type == HELLO_MN)
		printf("[net]\n 'HELLO_MN' message lost\n\n");
	else if(type == DATA)
	{
		printf("[net]\n 'DATA' message lost");
		printf(" (packet number: %d)\n\n", packet_number);
	}
    else if(type == DATA_ACK)
        printf("[net]\n 'DATA_ACK' message lost\n\n");
    else if(type == HELLO_ACK)
        printf("[net]\n 'HELLO_ACK' message lost\n\n");
	else
        printf("[net]\n Unknown type message lost\n\n");
}


void debug_net_message_received_connection(char* connection)
{
	printf("[net]\n Message received through '%s' connection\n\n",connection);
}


void debug_net_message_received(char* message)
{
	if(strcmp(message,"incorrect") == 0)
		printf("[net]\n Incorrect type of message received\n\n");
	else if(strcmp(message, "discarded")==0)
	{
		printf("[net]\n Already sending a message.");
	    printf(" Message received discarded.\n\n");
	}
	else printf("[net]\n '%s' received\n\n",message);
}


void debug_net_incorrect_checksum()
{
	printf("[net]\n Incorrect checksum invalid message\n\n");
}


void debug_net_info_net(int packets_sent, int packets_acknowledged)
{
	printf("[net]\n");
    printf(" Number of packets sent: %d\n", packets_sent);
	printf(" Number of packets acknowledged: %d\n\n", packets_acknowledged);
}

void debug_net_battery(uint16_t batt)
{
	u32_t tmp = 0;
        tmp = batt;
        tmp *= 5000;
        tmp /= 4096;
	printf("[net]\n  Battery sensor [%u mV]\n", tmp);
}

void debug_net_broadcast_burst()
{
	printf("[net]\n New Broadcast burst\n\n");
}

void debug_net_flooding_hello_bs()
{
	printf("[net]\n Forwarding 'HELLO_BS' (flooding)\n\n");
}

void debug_net_output_message_type(char *message)
{
	printf("Output message type: %s\n",message);
}

//STATE messages
void debug_state_current_state(char *state)
{
	printf("---\n[state]\n Current state '%s'\n---\n\n",state);
}


//FILEMAN messages
//TODO
void debug_filesys_init(FileManager *fman)
{
	if(fman->prefix == 'l')
    	printf("[file-man Local] initFileManager:\n");
    else printf("[file-man Net] initFileManager:\n");
    printf("  readFile = %d\n  readFD = %d\n", fman->readFile, fman->readFD);
    printf("  writeFile = %d\n  writeFD = %d\n", fman->writeFile, fman->writeFD);
    printf("  writeSampleNumber = %d\n", fman->writeSampleNumber);
    printf("  storedFiles = %d\n\n", fman->storedFiles);

}
void debug_filesys_write(FileManager *fman, int writeBytes,char fdOk)
{
	if(fman->prefix == 'l')
		printf("[file-man Local] write:\n");
	else printf("[file-man Net] write:\n");
	if(fdOk == TRUE)
	{
		printf("  writeFile = %d\n  writeFD = %d\n", fman->writeFile, fman->writeFD);
		if(writeBytes < 0)
			printf("  Error: File manager can't write into writeFile.\n\n");
		else printf("  writeBytes = %d\n\n", writeBytes);
	}
	else printf("  Error: writeFD is not a valid file descriptor.\n\n");
}

void debug_filesys_read(FileManager *fman, int readBytes,char fdOk)
{
	if(fman->prefix == 'l')
		printf("[file-man Local] read:\n");
	else printf("[file-man Net] read:\n");
	if(fdOk == TRUE)
	{
	  printf("  readFile = %d\n  readFD = %d\n", fman->readFile, fman->readFD);
      if(readBytes > 0)
      	printf("  readBytes = %d\n\n", readBytes);
      else printf("  Error: File manager can't read from readFile.\n\n");
	}
	else printf("  Error: readFD is not a valid file descriptor.\n\n");
}

void debug_filesys_updateReadFile(FileManager *fman, char fdOk)
{
	if(fman->prefix == 'l')
		printf("[file-man Local] updateReadFile:\n");
	else printf("[file-man Net] updateReadFile:\n");
	if(fman->storedFiles > 0)
	{
		printf("  readFile = %d\n  readFD = %d\n", fman->readFile, fman->readFD);
		printf("  storedFiles = %d\n\n", fman->storedFiles);
		if(fdOk == FALSE)
			printf("  Error: readFD is not a valid file descriptor.\n\n");
	}
	else printf("  Error: There's no any file stored in the file system.\n\n");
}

void debug_filesys_readSeek(FileManager *fman, int readFilePos, char fdOk)
{
	if(fman->prefix == 'l')
		printf("[file-man Local] readSeek:\n");
	else printf("[file-man Net] readSeek:\n");
	if(fdOk == TRUE)
	{
		printf("  readFile = %d\n  readFD = %d\n", fman->readFile, fman->readFD);
      	printf("  readFilePos = %d\n\n", readFilePos);
      	if(readFilePos < 0)
      		printf("  Error: File manager can't update the read file offset pointer.\n\n");	
	}
	else printf("  Error: readFD is not a valid file descriptor.\n\n");
}

void debug_filesys_updateWriteFile(FileManager *fman)
{
	if(fman->prefix == 'l')
		printf("[file-man Local] updateWriteFile:\n");
	else printf("[file-man Net] updateWriteFile:\n");
    printf("  readFile = %d\n  readFD = %d\n", fman->readFile, fman->readFD);
    printf("  writeFile = %d\n  writeFD = %d\n", fman->writeFile, fman->writeFD);
    printf("  writeSampleNumber = %d\n", fman->writeSampleNumber);
    printf("  storedFiles = %d\n\n", fman->storedFiles);
}

void debug_filesys_initOk(FileManager *fman,char initOk)
{
	if(fman->prefix == 'l')
		printf("[file-man Local]\n");
	else printf("[file-man Net]\n");
	if(initOk == TRUE)
		printf("  Init File Manager OK\n\n");
	else printf("  Init File Manager failed\n\n");
}

//EVENT messages
void debug_event_current_event(char* event)
{
	printf("[event]\n %s event\n\n",event);
}

//SENSOR messages
void debug_sensor_sample_measured(int sample_number)
{
	printf("[sensor]\n Sample measured");
	printf(" (sample number: %d)\n\n", sample_number);
}
