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
#define OUTPUT_LOG
#define MSS MAX_DATA_SIZE
struct PacketHead {
    u_int seq;
    u_int ack;
    u_short checkSum;
    u_short bufSize;
    u_char flag;
    u_char windows;

    PacketHead() {
        seq = ack = 0;
        checkSum = bufSize = 0;
        flag = 0;
        windows = 0;
    }
};

struct Packet {
    PacketHead head;
    char data[MAX_DATA_SIZE];
};

u_short CheckPacketSum(u_short *packet, int packetLen) {

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

void ShowPacket(Packet *pkt) {
#ifdef OUTPUT_LOG
    printf("[SYN:%d\tACK:%d\tFIN:%d\tEND:%d]SEQ:%d\tACK:%d\tLEN:%d\n",
           pkt->head.flag & 0x1, pkt->head.flag >> 1 & 0x1, pkt->head.flag >> 2 & 0x1, pkt->head.flag >> 3 & 0x1,
           pkt->head.seq, pkt->head.ack, pkt->head.bufSize);
#endif
}

enum {
    START_UP, AVOID, RECOVERY
};

string getRENOStageName(int stage){
    switch(stage){
        case START_UP:
            return "START_UP";
        case AVOID:
            return "AVOID";
        case RECOVERY:
            return "RECOVERY";
    }
}