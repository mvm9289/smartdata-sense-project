#include "zt-packet-mgmt.h"

unsigned short compute_checksum(Packet* my_packet) {
    unsigned char ch[32];
    ch[0] = my_packet->addr1 ^ my_packet->addr2 ^ my_packet->type ^
        my_packet->size ^ my_packet->counter;
    ch[1] = (my_packet->size>>8) ^ (my_packet->counter>>8);
    int i = 0;
    int j = 0;
    while (i < 32) {
        if(j < 23) {
            ch[i] = ch[i] ^ my_packet->data[i] ^ my_packet->reserved[j];
            j++;
        }
        else ch[i] = my_packet->data[i];
        i++;
    }
    
    i = 0;
    j = 31;
    
    while (i < 16) {
        ch[i] = ch[i] ^ ch[j];
        ++i;
        --j;
    }
    
    unsigned short checksum;
    i = 0;
    while (i < 16) {
        checksum = checksum << 1;
        checksum = checksum + ch[i];
    }
    
    return checksum;
}

unsigned char * mount_packet(Packet * my_packet) {
    unsigned char* packet;
    
    packet[0] = my_packet->addr1;
    packet[1] = my_packet->addr2;
    packet[2] = my_packet->type;
    packet[3] = my_packet->size;
    packet[4] = (my_packet->size>>8);
    packet[5] = my_packet->counter;
    packet[6] = (my_packet->counter>>8);
    
    int i = 7;
    int j = 0;
    while (j < 32) {
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
    
    return packet;
}

Packet unmount_packet(unsigned char* my_array) {
    Packet my_packet;
    my_packet.addr1 = my_array[0];
    my_packet.addr2 = my_array[1];
    my_packet.type = my_array[2];
    my_packet.size = (my_array[3]<<8) + my_array[4];
    my_packet.counter = (my_array[5]<<8) + my_array[6];
    
    int i = 7;
    int j = 0;
    
    while (j < 32) {
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
    
    my_packet.checksum = (my_array[i]<<8) + my_array[i+1];
    
    return my_packet;
}
