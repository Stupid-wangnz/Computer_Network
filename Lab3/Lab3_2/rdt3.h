//
// Created by LEGION on 2022-11-09.
//

#define SYN 0x1
#define ACK 0x2
#define FIN 0x4
#define END 0x8

#define MAX_DATA_SIZE 8192
#define MAX_SEQ 0xffff
using namespace std;

struct packetHead {
    u_int seq;
    u_int ack;
    u_short checkSum;
    u_short bufSize;
    char flag;

    packetHead() {
        seq = ack = 0;
        checkSum = bufSize = 0;
        flag = 0;
    }
};

struct packet {
    packetHead head;
    char data[MAX_DATA_SIZE];
};

u_short checkPacketSum(u_short *packet, int packetLen) {

    u_long sum = 0;
    int count = (packetLen + 1) / 2;

    u_short *temp = new u_short[count];
    memset(temp, 0, 2 * count);
    memcpy(temp, packet, packetLen);

    while (count--) {
        sum += *temp++;
        if (sum & 0xFFFF0000) {
            sum &= 0xFFFF;
            sum++;
        }
    }
    return ~(sum & 0xFFFF);
}

struct UDP_Connect {
    SOCKET *socket;
    SOCKADDR_IN *addr;
};