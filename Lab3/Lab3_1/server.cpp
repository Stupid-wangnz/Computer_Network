#include <iostream>
#include <WINSOCK2.h>
#include <time.h>
#include <fstream>
#include <cstdio>
#include <windows.h>
#include <iostream>
#include <thread>
#include "rdt3.h"

#pragma comment(lib, "ws2_32.lib")  //加载 ws2_32.dll
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
            cout << "第一次握手成功" << endl;
        }
        break;
    }

    packetHead head;
    head.flag |= ACK;
    head.flag |= SYN;
    head.checkSum = checkPacketSum((u_short *) &head, sizeof(packetHead));
    memcpy(buffer, &head, sizeof(packetHead));
    if (sendto(socket, buffer, sizeof(packetHead), 0, (sockaddr *) &addr, len) == -1) {
        cout << "第二次握手失败" << endl;
        return false;
    }
    cout << "第二次握手成功" << endl;
    u_long imode = 1;
    ioctlsocket(socket, FIONBIO, &imode);//非阻塞
    clock_t start = clock(); //开始计时
    while (recvfrom(socket, buffer, sizeof(head), 0, (sockaddr *) &addr, &len) <= 0) {
        if (clock() - start >= MAX_TIME) {
            cout << "超时重传" << endl;
            sendto(socket, buffer, sizeof(buffer), 0, (sockaddr *) &addr, len);
            start = clock();
        }
    }

    if ((((packetHead *) buffer)->flag & ACK) && (checkPacketSum((u_short *) buffer, sizeof(packetHead)) == 0)) {
        cout << "第三次握手成功" << endl;
    } else {
        cout << "第三次握手失败" << endl;
        return false;
    }
    imode = 0;
    ioctlsocket(socket, FIONBIO, &imode);//阻塞
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
                    cout<<"传输完毕"<<endl;
                    return fileLen;
                }

                memcpy(&pkt,pkt_buffer, sizeof(packet));

                if(pkt.head.seq==1|| checkPacketSum((u_short*)&pkt, sizeof(packet))!=0){
                    sendPkt= makePacket(1);
                    memcpy(pkt_buffer,&sendPkt, sizeof(packet));
                    sendto(socket,pkt_buffer, sizeof(packet),0,(SOCKADDR *)&addr,addrLen);
                    stage=0;
                    cout<<"收到重复的"<<index-1<<"号数据包"<<endl;
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

                cout<<"成功收到"<<index<<"号数据包，其长度是"<<dataLen<<endl;
                index++;
                break;
            case 1:
                recvfrom(socket, pkt_buffer, sizeof(packet), 0, (SOCKADDR *) &addr, &addrLen);

                memcpy(&pkt,pkt_buffer, sizeof(packetHead));
                if(pkt.head.flag&END){
                    cout<<"传输完毕"<<endl;
                    return fileLen;
                }

                memcpy(&pkt,pkt_buffer, sizeof(packet));

                if(pkt.head.seq==0|| checkPacketSum((u_short*)&pkt, sizeof(packet))!=0){
                    sendPkt= makePacket(0);
                    memcpy(pkt_buffer,&sendPkt, sizeof(packet));
                    sendto(socket,pkt_buffer, sizeof(packet),0,(SOCKADDR *)&addr,addrLen);
                    stage=1;
                    cout<<"收到重复的"<<index-1<<"号数据包"<<endl;
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

                cout<<"成功收到"<<index<<"号数据包，其长度是"<<dataLen<<endl;
                index++;

                break;
        }
    }
}

int main() {
    WSAData wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        //加载失败
        cout << "加载DLL失败" << endl;
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
        cout<<"打开文件出错"<<endl;
        return 0;
    }
    cout<<fileLen<<endl;
    outfile.write(fileBuffer,fileLen);
    outfile.close();

}