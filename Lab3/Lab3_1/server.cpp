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
double MAX_TIME = CLOCKS_PER_SEC / 10;

bool acceptClient(SOCKET &socket, SOCKADDR_IN &addr) {

    char *buffer = new char[sizeof(packetHead)];
    int len = sizeof(addr);
    while (1) {
        if (recvfrom(socket, buffer, sizeof(packetHead), 0, (SOCKADDR *) &addr, &len) <= 0)
            continue;
        if ((((packetHead *) buffer)->flag & SYN) && (checkPacketSum((u_short *) buffer, sizeof(packetHead)) == 0)) {
            cout << "��һ�����ֳɹ�" << endl;
        }
        break;
    }

    packetHead head;
    head.flag |= ACK;
    head.flag |= SYN;
    head.checkSum = checkPacketSum((u_short *) &head, sizeof(packetHead));
    memcpy(buffer, &head, sizeof(packetHead));
    if (sendto(socket, buffer, sizeof(packetHead), 0, (sockaddr *) &addr, len) == -1) {
        cout << "�ڶ�������ʧ��" << endl;
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

packet makePacket(int ack){
    packet pkt;
    pkt.head.ack=ack;
    pkt.head.flag|=ACK;
    pkt.head.checkSum= checkPacketSum((u_short*)&pkt, sizeof(packet));

    return pkt;
}

u_long recvFSM(char *fileBuffer, SOCKET &socket, SOCKADDR_IN &addr) {
    u_long fileLen=0;
    int stage = 0;

    int addrLen = sizeof(addr);
    char *pkt_buffer=new char[sizeof(packet)];
    packet pkt,sendPkt;
    int index=0;
    int dataLen;
    while (1) {
        memset(pkt_buffer,0, sizeof(packet));
        switch (stage) {
            case 0:
                recvfrom(socket, pkt_buffer, sizeof(packet), 0, (SOCKADDR *) &addr, &addrLen);

                memcpy(&pkt,pkt_buffer, sizeof(packetHead));
                if(pkt.head.flag&END){
                    cout<<"�������"<<endl;
                    return fileLen;
                }

                memcpy(&pkt,pkt_buffer, sizeof(packet));

                if(pkt.head.seq==1|| checkPacketSum((u_short*)&pkt, sizeof(packet))!=0){
                    sendPkt= makePacket(1);
                    memcpy(pkt_buffer,&sendPkt, sizeof(packet));
                    sendto(socket,pkt_buffer, sizeof(packet),0,(SOCKADDR *)&addr,addrLen);
                    stage=0;
                    cout<<"�յ��ظ���"<<index-1<<"�����ݰ�"<<endl;
                    break;
                }

                //correctly receive the seq0
                dataLen=pkt.head.bufSize;
                memcpy(fileBuffer+fileLen,pkt.data,dataLen);
                fileLen+=dataLen;

                //give back ack0
                sendPkt= makePacket(0);
                memcpy(pkt_buffer,&sendPkt, sizeof(packet));
                sendto(socket,pkt_buffer, sizeof(packet),0,(SOCKADDR *)&addr,addrLen);
                stage=1;

                cout<<"�ɹ��յ�"<<index<<"�����ݰ����䳤����"<<dataLen<<endl;
                index++;
                break;
            case 1:
                recvfrom(socket, pkt_buffer, sizeof(packet), 0, (SOCKADDR *) &addr, &addrLen);

                memcpy(&pkt,pkt_buffer, sizeof(packetHead));
                if(pkt.head.flag&END){
                    cout<<"�������"<<endl;
                    return fileLen;
                }

                memcpy(&pkt,pkt_buffer, sizeof(packet));

                if(pkt.head.seq==0|| checkPacketSum((u_short*)&pkt, sizeof(packet))!=0){
                    sendPkt= makePacket(0);
                    memcpy(pkt_buffer,&sendPkt, sizeof(packet));
                    sendto(socket,pkt_buffer, sizeof(packet),0,(SOCKADDR *)&addr,addrLen);
                    stage=1;
                    cout<<"�յ��ظ���"<<index-1<<"�����ݰ�"<<endl;
                    break;
                }

                //correctly receive the seq1
                dataLen=pkt.head.bufSize;
                memcpy(fileBuffer+fileLen,pkt.data,dataLen);
                fileLen+=dataLen;

                //give back ack1
                sendPkt= makePacket(1);
                memcpy(pkt_buffer,&sendPkt, sizeof(packet));
                sendto(socket,pkt_buffer, sizeof(packet),0,(SOCKADDR *)&addr,addrLen);
                stage=0;

                cout<<"�ɹ��յ�"<<index<<"�����ݰ����䳤����"<<dataLen<<endl;
                index++;

                break;
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
    SOCKET sockSrv = socket(AF_INET, SOCK_DGRAM, 0);

    SOCKADDR_IN addrSrv;
    addrSrv.sin_family = AF_INET;
    addrSrv.sin_port = htons(PORT);
    addrSrv.sin_addr.S_un.S_addr = inet_addr(ADDRSRV);
    bind(sockSrv, (SOCKADDR *) &addrSrv, sizeof(SOCKADDR));

    SOCKADDR_IN addrClient;
    acceptClient(sockSrv, addrClient);

    char fileBuffer[MAX_FILE_SIZE];
    u_long fileLen= recvFSM(fileBuffer,sockSrv,addrClient);


    string filename=R"(F:\Computer_network\Computer_Network\Lab3\Lab3_1\img_test_recv.bmp)";
    ofstream outfile(filename,ios::binary);
    if(!outfile.is_open()){
        cout<<"���ļ�����"<<endl;
        return 0;
    }
    cout<<fileLen<<endl;
    outfile.write(fileBuffer,fileLen);
    outfile.close();

}