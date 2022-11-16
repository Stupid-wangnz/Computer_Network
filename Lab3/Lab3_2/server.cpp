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

#define PORT 7878
#define ADDRSRV "127.0.0.1"
#define MAX_FILE_SIZE 1000000
double MAX_TIME = CLOCKS_PER_SEC;
static u_int initSeq = 0;

bool acceptClient(SOCKET &socket, SOCKADDR_IN &addr) {

    char *buffer = new char[sizeof(packetHead)];
    int len = sizeof(addr);
    recvfrom(socket, buffer, sizeof(packetHead), 0, (SOCKADDR *) &addr, &len);

    if ((((packetHead *) buffer)->flag & SYN) && (checkPacketSum((u_short *) buffer, sizeof(packetHead)) == 0))
        cout << "��һ�����ֳɹ�" << endl;
    else
        return false;
    initSeq = ((packetHead *) buffer)->seq;

    packetHead head;
    head.flag |= ACK;
    head.flag |= SYN;
    head.checkSum = checkPacketSum((u_short *) &head, sizeof(packetHead));
    memcpy(buffer, &head, sizeof(packetHead));

    if (sendto(socket, buffer, sizeof(packetHead), 0, (sockaddr *) &addr, len) == -1) {
        return false;
    }
    cout << "�ڶ������ֳɹ�" << endl;

    u_long imode = 1;
    ioctlsocket(socket, FIONBIO, &imode);//������

    clock_t start = clock(); //��ʼ��ʱ
    while (recvfrom(socket, buffer, sizeof(head), 0, (sockaddr *) &addr, &len) <= 0) {
        if (clock() - start >= MAX_TIME) {
            cout << "��ʱ�ش�" << endl;
            sendto(socket, buffer, sizeof(buffer), 0, (sockaddr *) &addr, len);
            start = clock();
        }
    }

    if ((((packetHead *) buffer)->flag & ACK) && (checkPacketSum((u_short *) buffer, sizeof(packetHead)) == 0)) {
        cout << "���������ֳɹ�" << endl;
    } else {
        cout << "����������ʧ��" << endl;
        return false;
    }
    imode = 0;
    ioctlsocket(socket, FIONBIO, &imode);//����
    return true;
}

bool disConnect(SOCKET &socket, SOCKADDR_IN &addr) {
    int addrLen = sizeof(addr);
    char *buffer = new char[sizeof(packetHead)];

    recvfrom(socket, buffer, sizeof(packetHead), 0, (SOCKADDR *) &addr, &addrLen);
    if ((((packetHead *) buffer)->flag & FIN) && (checkPacketSum((u_short *) buffer, sizeof(packetHead) == 0))) {
        cout << "�û��˶Ͽ�" << endl;
    } else {
        cout << "�������������ж�" << endl;
        return false;
    }

    packetHead closeHead;
    closeHead.flag = 0;
    closeHead.flag |= ACK;
    closeHead.checkSum = checkPacketSum((u_short *) &closeHead, sizeof(packetHead));
    memcpy(buffer, &closeHead, sizeof(packetHead));
    sendto(socket, buffer, sizeof(packetHead), 0, (SOCKADDR *) &addr, addrLen);

    closeHead.flag |= FIN;
    closeHead.checkSum = checkPacketSum((u_short *) &closeHead, sizeof(packetHead));
    memcpy(buffer, &closeHead, sizeof(packetHead));
    sendto(socket, buffer, sizeof(packetHead), 0, (SOCKADDR *) &addr, addrLen);

    u_long imode = 1;
    ioctlsocket(socket, FIONBIO, &imode);
    clock_t start = clock();
    while (recvfrom(socket, buffer, sizeof(packetHead), 0, (sockaddr *) &addr, &addrLen) <= 0) {
        if (clock() - start >= MAX_TIME) {
            memcpy(buffer, &closeHead, sizeof(packetHead));
            sendto(socket, buffer, sizeof(packetHead), 0, (SOCKADDR *) &addr, addrLen);
            start = clock();
        }
    }

    if ((((packetHead *) buffer)->flag & ACK) && (checkPacketSum((u_short *) buffer, sizeof(packetHead) == 0))) {
        cout << "���ӹر�" << endl;
    } else {
        cout << "�������������ж�" << endl;
        return false;
    }
    closesocket(socket);
    return true;
}

packet makePacket(u_int ack) {
    packet pkt;
    pkt.head.ack = ack;
    pkt.head.flag |= ACK;
    pkt.head.checkSum = checkPacketSum((u_short *) &pkt, sizeof(packet));

    return pkt;
}

u_long recvFSM(char *fileBuffer, SOCKET &socket, SOCKADDR_IN &addr) {
    u_long fileLen = 0;
    int addrLen = sizeof(addr);
    u_int expectedSeq = initSeq;
    int dataLen;

    char *pkt_buffer = new char[sizeof(packet)];
    packet recvPkt, sendPkt= makePacket(initSeq-1);

    while (true) {
        memset(pkt_buffer, 0, sizeof(packet));
        recvfrom(socket, pkt_buffer, sizeof(packet), 0, (SOCKADDR *) &addr, &addrLen);
        memcpy(&recvPkt,pkt_buffer, sizeof(packet));

        if (recvPkt.head.flag & END && checkPacketSum((u_short*)&recvPkt, sizeof(packetHead))==0) {
            cout << "�������" << endl;
            packetHead endPacket;
            endPacket.flag |= ACK;
            endPacket.checkSum = checkPacketSum((u_short *) &endPacket, sizeof(packetHead));
            memcpy(pkt_buffer, &endPacket, sizeof(packetHead));
            sendto(socket, pkt_buffer, sizeof(packetHead), 0, (SOCKADDR *) &addr, addrLen);
            return fileLen;
        }

        if(recvPkt.head.seq==expectedSeq && checkPacketSum((u_short*)&recvPkt, sizeof(packet))==0){
            //correctly receive the expected seq
            dataLen = recvPkt.head.bufSize;
            memcpy(fileBuffer + fileLen, recvPkt.data, dataLen);
            fileLen += dataLen;

            //give back ack=seq
            sendPkt = makePacket(expectedSeq);
            memcpy(pkt_buffer, &sendPkt, sizeof(packet));
            sendto(socket, pkt_buffer, sizeof(packet), 0, (SOCKADDR *) &addr, addrLen);
            expectedSeq=(expectedSeq+1)%MAX_SEQ;
            cout<<"recv"<<endl;
            continue;
        }
        cout<<"wait head:"<<expectedSeq<<endl;
        cout<<"recv head:"<<recvPkt.head.seq<<endl;
        memcpy(pkt_buffer, &sendPkt, sizeof(packet));
        sendto(socket, pkt_buffer, sizeof(packet), 0, (SOCKADDR *) &addr, addrLen);
    }
}

int main() {
    WSAData wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        //����ʧ��
        cout << "����DLLʧ��" << endl;
        return -1;
    }
    SOCKET sockSrv = socket(AF_INET, SOCK_DGRAM, 0);

    SOCKADDR_IN addrSrv;
    addrSrv.sin_family = AF_INET;
    addrSrv.sin_port = htons(PORT);
    addrSrv.sin_addr.S_un.S_addr = inet_addr(ADDRSRV);
    bind(sockSrv, (SOCKADDR *) &addrSrv, sizeof(SOCKADDR));

    SOCKADDR_IN addrClient;

    //�������ֽ�������
    if (!acceptClient(sockSrv, addrClient)) {
        cout << "����ʧ��" << endl;
        return 0;
    }

    char fileBuffer[MAX_FILE_SIZE];
    //�ɿ����ݴ������
    u_long fileLen = recvFSM(fileBuffer, sockSrv, addrClient);
    //�Ĵλ��ֶϿ�����
    if (!disConnect(sockSrv, addrClient)) {
        cout << "�Ͽ�ʧ��" << endl;
        return 0;
    }

    //д�븴���ļ�
    string filename = R"(F:\Computer_network\Computer_Network\Lab3\Lab3_2\test_recv.txt)";
    ofstream outfile(filename, ios::binary);
    if (!outfile.is_open()) {
        cout << "���ļ�����" << endl;
        return 0;
    }
    cout << fileLen << endl;
    outfile.write(fileBuffer, fileLen);
    outfile.close();
    getchar();
    return 1;
}