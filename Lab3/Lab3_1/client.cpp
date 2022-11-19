#include <iostream>
#include <WINSOCK2.h>
#include <time.h>
#include <fstream>
#include <windows.h>
#include <iostream>
#include <thread>
#include "rdt3.h"

#pragma comment(lib, "ws2_32.lib")  //加载 ws2_32.dll
#pragma comment(lib, "winmm.lib")

using namespace std;

#define PORT 7879
double MAX_TIME = CLOCKS_PER_SEC;
string ADDRSRV;
static int base=0;

bool connectToServer(SOCKET &socket, SOCKADDR_IN &addr) {
    int len = sizeof(addr);

    PacketHead head;
    head.flag |= SYN;
    head.seq = base;
    head.checkSum = CheckPacketSum((u_short *) &head, sizeof(head));

    char *buffer = new char[sizeof(head)];
    memcpy(buffer, &head, sizeof(head));
    sendto(socket, buffer, sizeof(head), 0, (sockaddr *) &addr, len);
    ShowPacket((Packet*)&head);
    cout << "[SYN_SEND]第一次握手成功" << endl;

    clock_t start = clock(); //开始计时
    while (recvfrom(socket, buffer, sizeof(head), 0, (sockaddr *) &addr, &len) <= 0) {
        if (clock() - start >= MAX_TIME) {
            memcpy(buffer, &head, sizeof(head));
            sendto(socket, buffer, sizeof(buffer), 0, (sockaddr *) &addr, len);
            start = clock();
        }
    }

    memcpy(&head, buffer, sizeof(head));
    ShowPacket((Packet*)&head);
    if ((head.flag & ACK) && (CheckPacketSum((u_short *) &head, sizeof(head)) == 0) && (head.flag & SYN)) {
        cout << "[ACK_RECV]第二次握手成功" << endl;
    } else {
        return false;
    }

    //服务器建立连接
    head.flag = 0;
    head.flag |= ACK;
    head.checkSum = 0;
    head.checkSum = (CheckPacketSum((u_short *) &head, sizeof(head)));
    memcpy(buffer, &head, sizeof(head));
    sendto(socket, buffer, sizeof(head), 0, (sockaddr *) &addr, len);
    ShowPacket((Packet*)&head);

    //等待两个MAX_TIME，如果没有收到消息说明ACK没有丢包
    start = clock();
    while (clock() - start <= 2 * MAX_TIME) {
        if (recvfrom(socket, buffer, sizeof(PacketHead), 0, (SOCKADDR *) &addr, &len) <= 0)
            continue;
        //说明这个ACK丢了
        memcpy(buffer, &head, sizeof(head));
        sendto(socket, buffer, sizeof(head), 0, (sockaddr *) &addr, len);
        start = clock();
    }
    cout<<"[ACK_SEND]三次握手成功"<<endl;
    cout << "[CONNECTED]成功与服务器建立连接，准备发送数据" << endl;
    return true;
}

bool disConnect(SOCKET &socket, SOCKADDR_IN &addr) {

    int addrLen = sizeof(addr);
    char *buffer = new char[sizeof(PacketHead)];
    PacketHead closeHead;
    closeHead.flag |= FIN;
    closeHead.checkSum = CheckPacketSum((u_short *) &closeHead, sizeof(PacketHead));
    memcpy(buffer, &closeHead, sizeof(PacketHead));

    ShowPacket((Packet*)&closeHead);
    if(sendto(socket, buffer, sizeof(PacketHead), 0, (SOCKADDR *) &addr, addrLen) != SOCKET_ERROR)
        cout<<"[FIN_SEND]第一次挥手成功"<<endl;
    else
        return false;

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
        cout << "[ACK_RECV]第二次挥手成功，客户端已经断开" << endl;
    } else {
        return false;
    }

    u_long imode = 0;
    ioctlsocket(socket, FIONBIO, &imode);//阻塞
    recvfrom(socket, buffer, sizeof(PacketHead), 0, (SOCKADDR *) &addr, &addrLen);
    memcpy(&closeHead,buffer,sizeof(PacketHead));
    ShowPacket((Packet*)buffer);
    if ((((PacketHead *) buffer)->flag & FIN) && (CheckPacketSum((u_short *) buffer, sizeof(PacketHead) == 0))) {
        cout << "[FIN_RECV]第三次挥手成功，服务器断开" << endl;
    } else {
        return false;
    }

    imode = 1;
    ioctlsocket(socket, FIONBIO, &imode);

    closeHead.flag = 0;
    closeHead.flag |= ACK;
    closeHead.checkSum = CheckPacketSum((u_short *) &closeHead, sizeof(PacketHead));

    memcpy(buffer, &closeHead, sizeof(PacketHead));
    sendto(socket, buffer, sizeof(PacketHead), 0, (SOCKADDR *) &addr, addrLen);
    ShowPacket((Packet*)&closeHead);
    start = clock();
    while (clock() - start <= 2 * MAX_TIME) {
        if (recvfrom(socket, buffer, sizeof(PacketHead), 0, (SOCKADDR *) &addr, &addrLen) <= 0)
            continue;
        //说明这个ACK丢了
        memcpy(buffer, &closeHead, sizeof(PacketHead));
        sendto(socket, buffer, sizeof(PacketHead), 0, (sockaddr *) &addr, addrLen);
        start = clock();
    }

    cout << "[ACK_SEND]第四次挥手成功，连接已关闭" << endl;
    closesocket(socket);
    return true;
}

Packet makePacket(int seq, char *data, int len) {
    Packet pkt;
    pkt.head.seq = seq;
    pkt.head.bufSize = len;
    memcpy(pkt.data, data, len);
    pkt.head.checkSum = CheckPacketSum((u_short *) &pkt, sizeof(Packet));
    return pkt;
}

#define min(a, b) a>b?b:a

void sendFSM(u_long len, char *fileBuffer, SOCKET &socket, SOCKADDR_IN &addr) {

    int packetNum = int(len / MAX_DATA_SIZE) + (len % MAX_DATA_SIZE ? 1 : 0);
    int index = 0;
    int packetDataLen = min(MAX_DATA_SIZE, len - index * MAX_DATA_SIZE);
    int stage = 0;

    int addrLen = sizeof(addr);
    clock_t start;

    char *data_buffer = new char[packetDataLen], *pkt_buffer = new char[sizeof(Packet)];
    Packet sendPkt, pkt;

    cout << "[SYSTEM]本次文件数据长度为" << len << "Bytes,需要传输" << packetNum << "个数据包" << endl;

    while (true) {
        if (index == packetNum) {
            PacketHead endPacket;
            endPacket.flag |= END;
            endPacket.checkSum = CheckPacketSum((u_short *) &endPacket, sizeof(PacketHead));
            memcpy(pkt_buffer, &endPacket, sizeof(PacketHead));
            sendto(socket, pkt_buffer, sizeof(PacketHead), 0, (SOCKADDR *) &addr, addrLen);

            ShowPacket((Packet*)&endPacket);
            while (recvfrom(socket, pkt_buffer, sizeof(PacketHead), 0, (SOCKADDR *) &addr, &addrLen) <= 0) {
                if (clock() - start >= MAX_TIME) {
                    memcpy(pkt_buffer, &endPacket, sizeof(PacketHead));
                    sendto(socket, pkt_buffer, sizeof(PacketHead), 0, (SOCKADDR *) &addr, addrLen);
                    start = clock();
                }
            }
            ShowPacket((Packet*)pkt_buffer);
            if (((PacketHead *) (pkt_buffer))->flag & ACK) {
                cout << "[END]文件传输完成" << endl;
            }

            return;
        }
        packetDataLen = min(MAX_DATA_SIZE, len - index * MAX_DATA_SIZE);
        switch (stage) {
            case 0:
                memcpy(data_buffer, fileBuffer + index * MAX_DATA_SIZE, packetDataLen);
                sendPkt = makePacket(0, data_buffer, packetDataLen);
                memcpy(pkt_buffer, &sendPkt, sizeof(Packet));
                sendto(socket, pkt_buffer, sizeof(Packet), 0, (SOCKADDR *) &addr, addrLen);
                ShowPacket((Packet*)&sendPkt);

                start = clock();//计时
                stage = 1;
                break;
            case 1:
                //time_out
                while (recvfrom(socket, pkt_buffer, sizeof(Packet), 0, (SOCKADDR *) &addr, &addrLen) <= 0) {
                    if (clock() - start >= MAX_TIME) {
                        memcpy(pkt_buffer, &sendPkt, sizeof(Packet));
                        sendto(socket, pkt_buffer, sizeof(Packet), 0, (SOCKADDR *) &addr, addrLen);
                        ShowPacket((Packet*)&sendPkt);
                        cout <<"[SYSTEM]"<< index << "号数据包超时重传" << endl;
                        start = clock();
                    }
                }

                memcpy(&pkt, pkt_buffer, sizeof(Packet));
                ShowPacket((Packet*)&pkt);
                if (pkt.head.ack == 1 || CheckPacketSum((u_short *) &pkt, sizeof(Packet)) != 0) {
                    stage = 1;
                    break;
                }
                stage = 2;
                index++;
                break;
            case 2:
                memcpy(data_buffer, fileBuffer + index * MAX_DATA_SIZE, packetDataLen);
                sendPkt = makePacket(1, data_buffer, packetDataLen);
                memcpy(pkt_buffer, &sendPkt, sizeof(Packet));
                sendto(socket, pkt_buffer, sizeof(Packet), 0, (SOCKADDR *) &addr, addrLen);
                ShowPacket((Packet*)&sendPkt);

                start = clock();//计时
                stage = 3;
                break;
            case 3:
                //time_out
                while (recvfrom(socket, pkt_buffer, sizeof(Packet), 0, (SOCKADDR *) &addr, &addrLen) <= 0) {
                    if (clock() - start >= MAX_TIME) {
                        memcpy(pkt_buffer, &sendPkt, sizeof(Packet));
                        sendto(socket, pkt_buffer, sizeof(Packet), 0, (SOCKADDR *) &addr, addrLen);
                        ShowPacket((Packet*)&sendPkt);
                        cout <<"[SYSTEM]"<< index << "号数据包超时重传" << endl;
                        start = clock();
                    }
                }
                memcpy(&pkt, pkt_buffer, sizeof(Packet));
                ShowPacket((Packet*)&pkt);
                if (pkt.head.ack == 0 || CheckPacketSum((u_short *) &pkt, sizeof(Packet)) != 0) {
                    stage = 3;
                    break;
                }
                stage = 0;
                index++;
                break;
            default:
                cout << "error" << endl;
                return;
        }
    }
}

int main() {
    WSAData wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        //加载失败
        cout << "[SYSTEM]加载DLL失败" << endl;
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

    if (!connectToServer(sockClient, addrSrv)) {
        cout << "[ERROR]连接失败" << endl;
        return 0;
    }

    string filename;
    cout << "[SYSTEM]请输入需要传输的文件名" << endl;
    cin >> filename;

    ifstream infile(filename, ifstream::binary);

    if (!infile.is_open()) {
        cout << "[ERROR]无法打开文件" << endl;
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
    cout << "[SYSTEM]开始传输" << endl;

    sendFSM(fileLen, fileBuffer, sockClient, addrSrv);

    if (!disConnect(sockClient, addrSrv)) {
        cout << "[ERROR]断开失败" << endl;
        return 0;
    }
    cout<<"文件传输完成"<<endl;
    return 1;
}