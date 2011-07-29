#ifndef __ZT_DEBUG_LIB_H__
#define __ZT_DEBUG_LIB_H__

#include "zt-packet-mgmt.h"
#include "zt-filesys-lib.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


//NET messages
void debug_net_sending_message(char* message);
void debug_net_sent_message();
void debug_net_packet_content(Packet *packet);
void debug_net_sending_WORKING_FILE(int packet_number);
void debug_net_timeout(int type,int packet_number);
void debug_net_max_attempts();
void debug_net_message_lost(int type, int packet_number);
void debug_net_message_received_connection(char* connection);
void debug_net_message_received(char* message);
void debug_net_incorrect_checksum();
void debug_net_info_net(int packets_sent, int packets_acknowledged);
void debug_net_battery(uint16_t batt);
void debug_net_broadcast_burst();
void debug_net_flooding_hello_bs();
void debug_net_output_message_type(char *message);

//STATE messages
void debug_state_current_state(char *state);

//FILEMAN messages
//TODO
void debug_filesys_init(FileManager *fman);
void debug_filesys_write(FileManager *fman, int writeBytes, char fdOk);
void debug_filesys_read(FileManager *fman, int readBytes,char fdOk);
void debug_filesys_updateReadFile(FileManager *fman, char fdOk);
void debug_filesys_readSeek(FileManager *fman, int readFilePos, char fdOk);
void debug_filesys_updateWriteFile(FileManager *fman);
void debug_filesys_initOk(FileManager *fman,char initOk);

//EVENT messages
void debug_event_current_event(char* event);

//SENSOR messages
void debug_sensor_sample_measured(int sample_number);



#endif /*__ZT_DEBUG_LIB_H__*/

