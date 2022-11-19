#include <iostream>
#include <WINSOCK2.h>
#include <time.h>
#include <fstream>
#include <windows.h>
#include <iostream>
#include <thread>
#include "rdt3.h"

#pragma comment(lib, "ws2_32.lib")  //���� ws2_32.dll
#pragma comment(lib, "winmm.lib")

using namespace std;

#define PORT 7878
#define ADDRSRV "127.0.0.1"
#define MAX_FILE_SIZE 100000000
char fileBuffer[MAX_FILE_SIZE];
double MAX_TIME = CLOCKS_PER_SEC;
static int base_stage = 0;

bool acceptClient(SOCKET &socket, SOCKADDR_IN &addr) {

    char *buffer = new char[sizeof(PacketHead)];
    int len = sizeof(addr);
    recvfrom(socket, buffer, sizeof(PacketHead), 0, (SOCKADDR *) &addr, &len);

    ShowPacket((Packet*)buffer);

    if ((((PacketHead *) buffer)->flag & SYN) && (CheckPacketSum((u_short *) buffer, sizeof(PacketHead)) == 0))
        cout << "[SYN_RECV]��һ�����ֳɹ�" << endl;
    else
        return false;
    base_stage = ((PacketHead *) buffer)->seq;

    PacketHead head;
    head.flag |= ACK;
    head.flag |= SYN;
    head.checkSum = CheckPacketSum((u_short *) &head, sizeof(PacketHead));
    memcpy(buffer, &head, sizeof(PacketHead));
    if (sendto(socket, buffer, sizeof(PacketHead), 0, (sockaddr *) &addr, len) == -1) {
        return false;
    }

    ShowPacket((Packet*)buffer);

    cout << "[SYN_ACK_SEND]�ڶ������ֳɹ�" << endl;
    u_long imode = 1;
    ioctlsocket(socket, FIONBIO, &imode);//������

    clock_t start = clock(); //��ʼ��ʱ
    while (recvfrom(socket, buffer, sizeof(head), 0, (sockaddr *) &addr, &len) <= 0) {
        if (clock() - start >= MAX_TIME) {
            sendto(socket, buffer, sizeof(buffer), 0, (sockaddr *) &addr, len);
            start = clock();
        }
    }

    ShowPacket((Packet*)buffer);

    if ((((PacketHead *) buffer)->flag & ACK) && (CheckPacketSum((u_short *) buffer, sizeof(PacketHead)) == 0)) {
        cout << "[ACK_RECV]���������ֳɹ�" << endl;
    } else {
        return false;
    }
    imode = 0;
    ioctlsocket(socket, FIONBIO, &imode);//����

    cout << "[CONNECTED]���û��˳ɹ��������ӣ�׼�������ļ�" << endl;
    return true;
}

bool disConnect(SOCKET &socket, SOCKADDR_IN &addr) {
    int addrLen = sizeof(addr);
    char *buffer = new char[sizeof(PacketHead)];

    recvfrom(socket, buffer, sizeof(PacketHead), 0, (SOCKADDR *) &addr, &addrLen);

    ShowPacket((Packet*)buffer);

    if ((((PacketHead *) buffer)->flag & FIN) && (CheckPacketSum((u_short *) buffer, sizeof(PacketHead) == 0))) {
        cout << "[SYS]��һ�λ�����ɣ��û��˶Ͽ�" << endl;
    } else {
        return false;
    }

    PacketHead closeHead;
    closeHead.flag = 0;
    closeHead.flag |= ACK;
    closeHead.checkSum = CheckPacketSum((u_short *) &closeHead, sizeof(PacketHead));
    memcpy(buffer, &closeHead, sizeof(PacketHead));
    sendto(socket, buffer, sizeof(PacketHead), 0, (SOCKADDR *) &addr, addrLen);

    ShowPacket((Packet*)buffer);
    cout<<"[SYS]�ڶ��λ������"<<endl;

    closeHead.flag = 0;
    closeHead.flag |= FIN;
    closeHead.checkSum = CheckPacketSum((u_short *) &closeHead, sizeof(PacketHead));
    memcpy(buffer, &closeHead, sizeof(PacketHead));
    sendto(socket, buffer, sizeof(PacketHead), 0, (SOCKADDR *) &addr, addrLen);

    ShowPacket((Packet*)buffer);
    cout<<"[SYS]�����λ������"<<endl;

    u_long imode = 1;
    ioctlsocket(socket, FIONBIO, &imode);
    clock_t start = clock();
    while (recvfrom(socket, buffer, sizeof(PacketHead), 0, (sockaddr *) &addr, &addrLen) <= 0) {
        if (clock() - start >= MAX_TIME) {
            memcpy(buffer, &closeHead, sizeof(PacketHead));
            sendto(socket, buffer, sizeof(PacketHead), 0, (SOCKADDR *) &addr, addrLen);
            start = clock();
        }
    }

    ShowPacket((Packet*)buffer);
    if ((((PacketHead *) buffer)->flag & ACK) && (CheckPacketSum((u_short *) buffer, sizeof(PacketHead) == 0))) {
        cout << "[SYS]���Ĵλ�����ɣ����ӹر�" << endl;
    } else {
        return false;
    }
    closesocket(socket);
    return true;
}

Packet makePacket(int ack) {
    Packet pkt;
    pkt.head.ack = ack;
    pkt.head.flag |= ACK;
    pkt.head.checkSum = CheckPacketSum((u_short *) &pkt, sizeof(Packet));

    return pkt;
}

u_long recvFSM(char *fileBuffer, SOCKET &socket, SOCKADDR_IN &addr) {
    u_long fileLen = 0;
    int stage = base_stage%2;

    int addrLen = sizeof(addr);
    char *pkt_buffer = new char[sizeof(Packet)];
    Packet pkt, sendPkt;
    int index = 0;
    int dataLen;
    while (1) {
        memset(pkt_buffer, 0, sizeof(Packet));
        switch (stage) {
            case 0:
                recvfrom(socket, pkt_buffer, sizeof(Packet), 0, (SOCKADDR *) &addr, &addrLen);

                memcpy(&pkt, pkt_buffer, sizeof(PacketHead));
                ShowPacket(&pkt);

                if (pkt.head.flag & END) {
                    cout << "[SYSTEM]�������" << endl;
                    PacketHead endPacket;
                    endPacket.flag |= ACK;
                    endPacket.checkSum = CheckPacketSum((u_short *) &endPacket, sizeof(PacketHead));
                    memcpy(pkt_buffer, &endPacket, sizeof(PacketHead));
                    sendto(socket, pkt_buffer, sizeof(PacketHead), 0, (SOCKADDR *) &addr, addrLen);
                    ShowPacket((Packet*)&endPacket);
                    return fileLen;
                }

                memcpy(&pkt, pkt_buffer, sizeof(Packet));

                if (pkt.head.seq == 1 || CheckPacketSum((u_short *) &pkt, sizeof(Packet)) != 0) {
                    sendPkt = makePacket(1);
                    memcpy(pkt_buffer, &sendPkt, sizeof(Packet));
                    sendto(socket, pkt_buffer, sizeof(Packet), 0, (SOCKADDR *) &addr, addrLen);
                    stage = 0;
                    cout << "[SYSTEM]�յ��ظ���" << index - 1 << "�����ݰ�����������" << endl;
                    break;
                }

                //correctly receive the seq0
                dataLen = pkt.head.bufSize;
                memcpy(fileBuffer + fileLen, pkt.data, dataLen);
                fileLen += dataLen;

                //give back ack0
                sendPkt = makePacket(0);
                memcpy(pkt_buffer, &sendPkt, sizeof(Packet));
                sendto(socket, pkt_buffer, sizeof(Packet), 0, (SOCKADDR *) &addr, addrLen);
                stage = 1;
                ShowPacket(&sendPkt);
                //cout<<"�ɹ��յ�"<<index<<"�����ݰ����䳤����"<<dataLen<<endl;
                index++;
                break;
            case 1:
                recvfrom(socket, pkt_buffer, sizeof(Packet), 0, (SOCKADDR *) &addr, &addrLen);
                memcpy(&pkt, pkt_buffer, sizeof(PacketHead));

                ShowPacket(&pkt);

                if (pkt.head.flag & END) {
                    cout << "[SYSTEM]�������" << endl;
                    PacketHead endPacket;
                    endPacket.flag |= ACK;
                    endPacket.checkSum = CheckPacketSum((u_short *) &endPacket, sizeof(PacketHead));
                    memcpy(pkt_buffer, &endPacket, sizeof(PacketHead));
                    sendto(socket, pkt_buffer, sizeof(PacketHead), 0, (SOCKADDR *) &addr, addrLen);
                    ShowPacket((Packet*)&endPacket);
                    return fileLen;
                }

                memcpy(&pkt, pkt_buffer, sizeof(Packet));

                if (pkt.head.seq == 0 || CheckPacketSum((u_short *) &pkt, sizeof(Packet)) != 0) {
                    sendPkt = makePacket(0);
                    memcpy(pkt_buffer, &sendPkt, sizeof(Packet));
                    sendto(socket, pkt_buffer, sizeof(Packet), 0, (SOCKADDR *) &addr, addrLen);
                    stage = 1;
                    cout << "[SYSTEM]�յ��ظ���" << index - 1 << "�����ݰ�����������" << endl;
                    break;
                }

                //correctly receive the seq1
                dataLen = pkt.head.bufSize;
                memcpy(fileBuffer + fileLen, pkt.data, dataLen);
                fileLen += dataLen;

                //give back ack1
                sendPkt = makePacket(1);
                memcpy(pkt_buffer, &sendPkt, sizeof(Packet));
                sendto(socket, pkt_buffer, sizeof(Packet), 0, (SOCKADDR *) &addr, addrLen);
                stage = 0;
                ShowPacket(&sendPkt);
                //cout<<"�ɹ��յ�"<<index<<"�����ݰ����䳤����"<<dataLen<<endl;
                index++;

                break;
        }
    }
}

int main() {
    WSAData wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        //����ʧ��
        cout << "[ERROR]����DLLʧ��" << endl;
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
        cout << "[ERROR]����ʧ��" << endl;
        return 0;
    }

    //char fileBuffer[MAX_FILE_SIZE];
    //�ɿ����ݴ������
    u_long fileLen = recvFSM(fileBuffer, sockSrv, addrClient);
    //�Ĵλ��ֶϿ�����
    if (!disConnect(sockSrv, addrClient)) {
        cout << "[ERROR]�Ͽ�ʧ��" << endl;
        return 0;
    }

    //д�븴���ļ�
    string filename = R"(F:\Computer_network\Computer_Network\Lab3\Lab3_1\workfile3_1\1_recv.jpg)";
    ofstream outfile(filename, ios::binary);
    if (!outfile.is_open()) {
        cout << "[ERROR]���ļ�����" << endl;
        return 0;
    }
    cout << fileLen << endl;
    outfile.write(fileBuffer, fileLen);
    outfile.close();

    cout << "[FINISHED]�ļ��������" << endl;
    return 1;
}