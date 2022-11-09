#include <iostream>
#include <WINSOCK2.h>
#include <time.h>
#include <fstream>
#include <cstdio>
#include <windows.h>
#include <iostream>
#include <thread>
#include <math.h>
#include "rdt3.h"

#pragma comment(lib, "ws2_32.lib")  //加载 ws2_32.dll
#pragma comment(lib, "winmm.lib")

using namespace std;

#define PORT 7878
double MAX_TIME = CLOCKS_PER_SEC / 1000;
string ADDRSRV;


bool connectToServer(SOCKET &socket, SOCKADDR_IN &addr) {
    packetHead head;
    head.flag |= SYN;
    head.checkSum = checkPacketSum((u_short *) &head, sizeof(head));

    char *buffer = new char[sizeof(head)];
    memcpy(buffer, &head, sizeof(head));

    int len = sizeof(addr);

    if (sendto(socket, buffer, sizeof(head), 0, (sockaddr *) &addr, len) == -1) {
        cout << "第一次握手失败" << endl;
        return false;
    }
    cout << "第一次握手成功" << endl;
    clock_t start = clock(); //开始计时
    while (recvfrom(socket, buffer, sizeof(head), 0, (sockaddr *) &addr, &len) <= 0) {
        if (clock() - start >= MAX_TIME) {
            cout << "超时重传" << endl;
            sendto(socket, buffer, sizeof(buffer), 0, (sockaddr *) &addr, len);
            start = clock();
        }
    }

    memcpy(&head, buffer, sizeof(head));
    if ((head.flag & ACK) && (checkPacketSum((u_short *) &head, sizeof(head)) == 0)) {
        cout << "第二次握手成功" << endl;
    } else {
        cout << "第二次握手失败" << endl;
        return false;
    }

    if (head.flag & SYN) {
        head.flag = 0;
        head.flag |= ACK;
        head.checkSum = 0;
        head.checkSum = (checkPacketSum((u_short *) &head, sizeof(head)));
    }

    if (sendto(socket, buffer, sizeof(head), 0, (sockaddr *) &addr, len) == -1) {
        cout << "第三次握手失败" << endl;
        return false;
    }

    cout << "成功与服务器建立连接，可发送数据" << endl;
    return true;
}

packet makePacket(int seq, char *data, int len) {
    packet pkt;
    pkt.head.seq = seq;
    pkt.head.bufSize = len;
    memcpy(pkt.data, data, len);
    pkt.head.checkSum = checkPacketSum((u_short *) &pkt, sizeof(packet));
    return pkt;
}
#define min(a,b) a>b?b:a
void sendFSM(u_long len, char *fileBuffer, SOCKET &socket, SOCKADDR_IN &addr) {

    int packetNum = int(len / MAX_DATA_SIZE) + (len % MAX_DATA_SIZE ? 1 : 0);
    int index = 0;
    int packetDataLen = min(MAX_DATA_SIZE,len-index*MAX_DATA_SIZE);
    int stage = 0;

    int addrLen = sizeof(addr);
    clock_t start;

    char *data_buffer=new char[packetDataLen], *pkt_buffer = new char[sizeof(packet)];
    packet sendPkt,pkt;

    cout<<"本次文件数据长度为"<<len<<"Bytes,需要传输"<<packetNum<<"个数据包"<<endl;

    while (true) {

        if (index == packetNum){
            cout<<"传输完成"<<endl;
            packetHead endPacket;
            endPacket.flag|=END;
            endPacket.checkSum= checkPacketSum((u_short*)&endPacket, sizeof(packetHead));
            memcpy(pkt_buffer,&endPacket, sizeof(packetHead));
            sendto(socket, pkt_buffer, sizeof(packetHead), 0, (SOCKADDR *) &addr, addrLen);
            return;
        }
        packetDataLen = min(MAX_DATA_SIZE,len-index*MAX_DATA_SIZE);
        switch (stage) {
            case 0:
                memcpy(data_buffer, fileBuffer + index * MAX_DATA_SIZE, packetDataLen);
                sendPkt = makePacket(0, data_buffer, packetDataLen);

                memcpy(pkt_buffer, &sendPkt, sizeof(packet));
                sendto(socket, pkt_buffer, sizeof(packet), 0, (SOCKADDR *) &addr, addrLen);

                start = clock();//计时
                stage = 1;
                break;
            case 1:
                //time_out
                while (recvfrom(socket, pkt_buffer, sizeof(packet), 0, (SOCKADDR *) &addr, &addrLen) <= 0) {
                    if (clock() - start >= MAX_TIME) {
                        sendto(socket, pkt_buffer, sizeof(packet), 0, (SOCKADDR *) &addr, addrLen);
                        start = clock();
                    }
                }
                memcpy(&pkt, pkt_buffer, sizeof(packet));
                if (pkt.head.ack == 1 || checkPacketSum((u_short *) &pkt, sizeof(packet)) != 0) {
                    stage = 1;
                    break;
                }
                stage = 2;
                cout<<index<<"号数据包传输成功，传输了"<<packetDataLen<<"Bytes数据"<<endl;
                index++;
                break;
            case 2:
                memcpy(data_buffer, fileBuffer + index * MAX_DATA_SIZE, packetDataLen);
                sendPkt = makePacket(1, data_buffer, packetDataLen);
                memcpy(pkt_buffer, &sendPkt, sizeof(packet));
                sendto(socket, pkt_buffer, sizeof(packet), 0, (SOCKADDR *) &addr, addrLen);

                start = clock();//计时
                stage = 3;
                break;
            case 3:
                //time_out
                while (recvfrom(socket, pkt_buffer, sizeof(packet), 0, (SOCKADDR *) &addr, &addrLen) <= 0) {
                    if (clock() - start >= MAX_TIME) {
                        sendto(socket, pkt_buffer, sizeof(packet), 0, (SOCKADDR *) &addr, addrLen);
                        start = clock();
                    }
                }
                memcpy(&pkt, pkt_buffer, sizeof(packet));
                if (pkt.head.ack == 0 || checkPacketSum((u_short *) &pkt, sizeof(packet)) != 0) {
                    stage = 3;
                    break;
                }
                stage = 0;
                cout<<index<<"号数据包传输成功，传输了"<<packetDataLen<<"Bytes数据"<<endl;
                index++;
                break;
            default:
                cout<<"error"<<endl;
                return;
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
    SOCKET sockClient = socket(AF_INET, SOCK_DGRAM, 0);
    u_long imode = 1;
    ioctlsocket(sockClient, FIONBIO, &imode);//非阻塞

    cout << "[NOT CONNECTED]请输入聊天服务器的地址" << endl;
    cin >> ADDRSRV;

    SOCKADDR_IN addrSrv;
    addrSrv.sin_family = AF_INET;
    addrSrv.sin_port = htons(PORT);
    addrSrv.sin_addr.S_un.S_addr = inet_addr(ADDRSRV.c_str());
    connectToServer(sockClient,addrSrv);

    string filename;
    cout << "请输入文件名" << endl;
    cin >> filename;

    ifstream infile(filename, ifstream::binary);

    if (!infile.is_open()) {
        cout << "无法打开文件" << endl;
        return 0;
    }

    infile.seekg(0, infile.end);
    u_long fileLen = infile.tellg();
    infile.seekg(0, infile.beg);
    cout<<fileLen<<endl;

    char *fileBuffer = new char[fileLen];
    infile.read(fileBuffer, fileLen);
    infile.close();
    //cout.write(fileBuffer,fileLen);

    sendFSM(fileLen,fileBuffer,sockClient,addrSrv);
}