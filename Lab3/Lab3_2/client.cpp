#include <iostream>
#include <WINSOCK2.h>
#include <time.h>
#include <fstream>
#include <cstdio>
#include <windows.h>
#include <iostream>
#include <thread>
#include "rdt3.h"

#pragma comment(lib, "ws2_32.lib")  //���� ws2_32.dll
#pragma comment(lib, "winmm.lib")

using namespace std;
#define min(a, b) a>b?b:a
#define max(a, b) a>b?a:b
#define PORT 7879
double MAX_TIME = CLOCKS_PER_SEC;

string ADDRSRV;
static int windowSize = 16;
static unsigned int base = 0;//���ֽ׶�ȷ���ĳ�ʼ���к�
u_int waitingNum(u_int baseSeq, u_int nextSeq) {
    if (nextSeq >= baseSeq)
        return nextSeq - baseSeq;
    return nextSeq + MAX_SEQ - baseSeq;
}

bool connectToServer(SOCKET &socket, SOCKADDR_IN &addr) {
    int len = sizeof(addr);

    packetHead head;
    head.flag |= SYN;
    head.seq = base = 1;
    head.checkSum = checkPacketSum((u_short *) &head, sizeof(head));

    char *buffer = new char[sizeof(head)];
    memcpy(buffer, &head, sizeof(head));
    sendto(socket, buffer, sizeof(head), 0, (sockaddr *) &addr, len);
    cout << "��һ�����ֳɹ�" << endl;

    clock_t start = clock(); //��ʼ��ʱ
    while (recvfrom(socket, buffer, sizeof(head), 0, (sockaddr *) &addr, &len) <= 0) {
        if (clock() - start >= MAX_TIME) {
            memcpy(buffer, &head, sizeof(head));
            sendto(socket, buffer, sizeof(buffer), 0, (sockaddr *) &addr, len);
            start = clock();
        }
    }

    memcpy(&head, buffer, sizeof(head));
    if ((head.flag & ACK) && (checkPacketSum((u_short *) &head, sizeof(head)) == 0)) {
        cout << "�ڶ������ֳɹ�" << endl;
    } else {
        cout << "�ڶ�������ʧ��" << endl;
        return false;
    }

    //��������������
    if (head.flag & SYN) {
        head.flag = 0;
        head.flag |= ACK;
        head.checkSum = 0;
        head.checkSum = (checkPacketSum((u_short *) &head, sizeof(head)));
    } else {
        cout << "���ӹ��̳���" << endl;
        return false;
    }
    memcpy(buffer, &head, sizeof(head));
    sendto(socket, buffer, sizeof(head), 0, (sockaddr *) &addr, len);

    //�ȴ�����MAX_TIME�����û���յ���Ϣ˵��ACKû�ж���
    start = clock();
    while (clock() - start <= 2 * MAX_TIME) {
        if (recvfrom(socket, buffer, sizeof(packetHead), 0, (SOCKADDR *) &addr, &len) <= 0)
            continue;
        //˵�����ACK����
        memcpy(buffer, &head, sizeof(head));
        sendto(socket, buffer, sizeof(head), 0, (sockaddr *) &addr, len);
        start = clock();
    }
    cout << "����������ɣ��ɹ���������������ӣ��ɷ�������" << endl;
    return true;
}

bool disConnect(SOCKET &socket, SOCKADDR_IN &addr) {

    int addrLen = sizeof(addr);
    char *buffer = new char[sizeof(packetHead)];
    packetHead closeHead;
    closeHead.flag |= FIN;
    closeHead.checkSum = checkPacketSum((u_short *) &closeHead, sizeof(packetHead));

    memcpy(buffer, &closeHead, sizeof(packetHead));
    sendto(socket, buffer, sizeof(packetHead), 0, (SOCKADDR *) &addr, addrLen);

    clock_t start = clock();
    while (recvfrom(socket, buffer, sizeof(packetHead), 0, (sockaddr *) &addr, &addrLen) <= 0) {
        if (clock() - start >= MAX_TIME) {
            memcpy(buffer, &closeHead, sizeof(packetHead));
            sendto(socket, buffer, sizeof(packetHead), 0, (SOCKADDR *) &addr, addrLen);
            start = clock();
        }
    }

    if ((((packetHead *) buffer)->flag & ACK) && (checkPacketSum((u_short *) buffer, sizeof(packetHead) == 0))) {
        cout << "�ͻ����Ѿ��Ͽ�" << endl;
    } else {
        cout << "�������������ж�" << endl;
        return false;
    }

    u_long imode = 0;
    ioctlsocket(socket, FIONBIO, &imode);//����

    recvfrom(socket, buffer, sizeof(packetHead), 0, (SOCKADDR *) &addr, &addrLen);

    if ((((packetHead *) buffer)->flag & FIN) && (checkPacketSum((u_short *) buffer, sizeof(packetHead) == 0))) {
        cout << "�������Ͽ�" << endl;
    } else {
        cout << "�������������ж�" << endl;
        return false;
    }

    imode = 1;
    ioctlsocket(socket, FIONBIO, &imode);

    closeHead.flag = 0;
    closeHead.flag |= ACK;
    closeHead.checkSum = checkPacketSum((u_short *) &closeHead, sizeof(packetHead));

    memcpy(buffer, &closeHead, sizeof(packetHead));
    sendto(socket, buffer, sizeof(packetHead), 0, (SOCKADDR *) &addr, addrLen);
    start = clock();
    while (clock() - start <= 2 * MAX_TIME) {
        if (recvfrom(socket, buffer, sizeof(packetHead), 0, (SOCKADDR *) &addr, &addrLen) <= 0)
            continue;
        //˵�����ACK����
        memcpy(buffer, &closeHead, sizeof(packetHead));
        sendto(socket, buffer, sizeof(packetHead), 0, (sockaddr *) &addr, addrLen);
        start = clock();
    }

    cout << "���ӹر�" << endl;
    closesocket(socket);
    return true;
}

packet makePacket(u_int seq, char *data, int len) {
    packet pkt;
    pkt.head.seq = seq;
    pkt.head.bufSize = len;
    memcpy(pkt.data, data, len);
    pkt.head.checkSum = checkPacketSum((u_short *) &pkt, sizeof(packet));
    return pkt;
}

bool inWindows(u_int seq) {
    if (seq >= base && seq < base + windowSize)
        return true;
    if (seq < base && seq < ((base + windowSize) % MAX_SEQ))
        return true;

    return false;
}

void sendFSM(u_long len, char *fileBuffer, SOCKET &socket, SOCKADDR_IN &addr) {

    int packetNum = int(len / MAX_DATA_SIZE) + (len % MAX_DATA_SIZE ? 1 : 0);
    int sendIndex = 0, recvIndex = 0;
    int packetDataLen;
    int addrLen = sizeof(addr);
    clock_t start;
    bool stopTimer = false;
    char *data_buffer = new char[packetDataLen], *pkt_buffer = new char[sizeof(packet)];
    packet recvPkt;
    u_int nextSeqNum = base;
    packet sendPkt[windowSize];
    cout << "�����ļ����ݳ���Ϊ" << len << "Bytes,��Ҫ����" << packetNum << "�����ݰ�" << endl;

    while (true) {
        if (recvIndex == packetNum) {
            packetHead endPacket;
            endPacket.flag |= END;
            endPacket.checkSum = checkPacketSum((u_short *) &endPacket, sizeof(packetHead));
            memcpy(pkt_buffer, &endPacket, sizeof(packetHead));
            sendto(socket, pkt_buffer, sizeof(packetHead), 0, (SOCKADDR *) &addr, addrLen);

            while (recvfrom(socket, pkt_buffer, sizeof(packetHead), 0, (SOCKADDR *) &addr, &addrLen) <= 0) {
                if (clock() - start >= MAX_TIME) {
                    start = clock();
                    goto resend;
                }
            }

            if (((packetHead *) (pkt_buffer))->flag & ACK &&
                checkPacketSum((u_short *) pkt_buffer, sizeof(packetHead)) == 0) {
                cout << "�ļ��������" << endl;
                return;
            }

            resend:
            continue;
        }

        packetDataLen = min(MAX_DATA_SIZE, len - sendIndex * MAX_DATA_SIZE);

        if (inWindows(nextSeqNum) && sendIndex < packetNum) {

            memcpy(data_buffer, fileBuffer + sendIndex * MAX_DATA_SIZE, packetDataLen);
            sendPkt[(int) waitingNum(base, nextSeqNum)] = makePacket(nextSeqNum, data_buffer, packetDataLen);
            memcpy(pkt_buffer, &sendPkt[(int) waitingNum(base, nextSeqNum)], sizeof(packet));
            sendto(socket, pkt_buffer, sizeof(packet), 0, (SOCKADDR *) &addr, addrLen);

            if (base == nextSeqNum) {
                start = clock();
                stopTimer = false;
            }
            nextSeqNum = (nextSeqNum + 1) % MAX_SEQ;
            sendIndex++;
            cout << sendIndex << "�����ݰ��Ѿ�����" << endl;
        }

        while (recvfrom(socket, pkt_buffer, sizeof(packet), 0, (SOCKADDR *) &addr, &addrLen) > 0) {
            memcpy(&recvPkt, pkt_buffer, sizeof(packet));
            //corrupt
            if (checkPacketSum((u_short *) &recvPkt, sizeof(packet)) != 0 || !(recvPkt.head.flag & ACK))
                goto time_out;
            //not corrupt
            cout << "base:" << base << " nextSeq:" << nextSeqNum << " endWindow:" << base + windowSize << endl;
            if (base < (recvPkt.head.ack + 1)) {
                int d = recvPkt.head.ack + 1 - base;
                for (int i = 0; i < (int) waitingNum(base, nextSeqNum) - d; i++) {
                    sendPkt[i] = sendPkt[i + d];
                }
                recvIndex+=d;
            }
            base = (max((recvPkt.head.ack + 1), base)) % MAX_SEQ;
            if (base == nextSeqNum)
                stopTimer = true;
            else {
                start = clock();
                stopTimer = false;
            }

        }

        time_out:
        if (!stopTimer && clock() - start >= MAX_TIME) {
            cout << "resend" << endl;
            for (int i = 0; i < (int) waitingNum(base, nextSeqNum); i++) {
                memcpy(pkt_buffer, &sendPkt[i], sizeof(packet));
                sendto(socket, pkt_buffer, sizeof(packet), 0, (SOCKADDR *) &addr, addrLen);
            }
            start = clock();
            stopTimer = false;
        }
    }
}

int main() {
    WSAData wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        //����ʧ��
        cout << "����DLLʧ��" << endl;
        return -1;
    }
    SOCKET sockClient = socket(AF_INET, SOCK_DGRAM, 0);

    u_long imode = 1;
    ioctlsocket(sockClient, FIONBIO, &imode);//������

    cout << "[NOT CONNECTED]����������������ĵ�ַ" << endl;
    cin >> ADDRSRV;

    SOCKADDR_IN addrSrv;
    addrSrv.sin_family = AF_INET;
    addrSrv.sin_port = htons(PORT);
    addrSrv.sin_addr.S_un.S_addr = inet_addr(ADDRSRV.c_str());

    if (!connectToServer(sockClient, addrSrv)) {
        cout << "����ʧ��" << endl;
        return 0;
    }

    string filename;
    cout << "��������Ҫ������ļ���" << endl;
    cin >> filename;

    ifstream infile(filename, ifstream::binary);

    if (!infile.is_open()) {
        cout << "�޷����ļ�" << endl;
        return 0;
    }

    infile.seekg(0, infile.end);
    u_long fileLen = infile.tellg();
    infile.seekg(0, infile.beg);
    cout << fileLen << endl;

    char *fileBuffer = new char[fileLen];
    infile.read(fileBuffer, fileLen);
    infile.close();
    //cout.write(fileBuffer,fileLen);
    cout << "��ʼ����" << endl;

    sendFSM(fileLen, fileBuffer, sockClient, addrSrv);

    if (!disConnect(sockClient, addrSrv)) {
        cout << "�Ͽ�ʧ��" << endl;
        return 0;
    }
    return 1;
}