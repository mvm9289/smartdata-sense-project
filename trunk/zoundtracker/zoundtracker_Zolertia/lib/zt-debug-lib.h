#ifndef __ZT_DEBUG_LIB_H__
#define __ZT_DEBUG_LIB_H__

#include "zt-packet-mgmt.h"

//NET messages
void debug_net_sending_message(char* message);
void debug_net_packet_content(Packet *packet);
void debug_net_sending_WORKING_FILE(int packet_number);
void debug_net_timeout(int type,int packet_number);
void debug_net_max_attempts();
void debug_net_message_lost(int type, int packet_number);
void debug_net_message_received_connection(char* connection);
void debug_net_message_received(char* message);
void debug_net_incorrect_checksum();
void debug_net_info_net(int packets_sent, int packets_acknowledged);

//STATE messages
void debug_state_current_state(char *state);

//FILEMAN messages
//TODO

//EVENT messages
void debug_event_timer_expired();

//SENSOR messages
void debug_sensor_sample_measured(int sample_number);



#endif /*__ZT_DEBUG_LIB_H__*/

