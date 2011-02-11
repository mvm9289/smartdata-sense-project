#include "zt-packet-mgmt.h"

unsigned short compute_checksum(Packet* my_packet) {
    unsigned short ch = my_packet->addr1;
    
    ch = ch ^ my_packet->addr2 ^ my_packet->type ^
        my_packet->size ^ my_packet->counter;

    int i = 0;
    int max = (my_packet->size - my_packet->counter);
    if(max > 32) max = 32;
    while (i < max) {
        ch = ch ^ my_packet->data[i];
        i++;
    }
    
    return ch;
}

void mount_packet(Packet * my_packet, unsigned char* packet) {
    int i;
    for (i = 0; i < PACKET_SIZE; i++) packet[i] = 0;
    
    packet[0] = my_packet->addr1;
    packet[1] = my_packet->addr2;
    packet[2] = my_packet->type;
    packet[3] = my_packet->size;
    packet[4] = (my_packet->size>>8);
    packet[5] = my_packet->counter;
    packet[6] = (my_packet->counter>>8);
    
    i = 7;
    int j = 0;
    int max = (my_packet->size - my_packet->counter);
    if(max > 32) max = 32;

    while (j < max) {
        packet[i] = my_packet->data[j];
        i++;
        j++;
    }
    
    j = 0;
    while (j < 23) {
        packet[i] = my_packet->reserved[j];
        i++;
        j++;
    }
    
    packet[i] = my_packet->checksum;
    packet[i+1] = (my_packet->checksum>>8);
}

Packet unmount_packet(unsigned char* my_array) {
    Packet my_packet;
    my_packet.addr1 = my_array[0];
    my_packet.addr2 = my_array[1];
    my_packet.type = my_array[2];
    my_packet.size = (my_array[4]<<8) | my_array[3];
    my_packet.counter = (my_array[6]<<8) | my_array[5];
    
    int i = 7;
    int j = 0;
    
    int max = (my_packet.size - my_packet.counter);
    if(max > 32) max = 32;
    
    while (j < max) {
        my_packet.data[j] = my_array[i];
        i++;
        j++;
    } 
    
    j = 0;
    
    while (j < 23) {
        my_packet.reserved[j] = my_array[i];
        i++;
        j++;
    }
    
    my_packet.checksum = (my_array[i+1]<<8) | my_array[i];
    
    return my_packet;
}
