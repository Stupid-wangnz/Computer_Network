#include <iostream>
#include <WINSOCK2.h>
#include <time.h>
#include <fstream>
#include <cstdio>
#include <windows.h>
#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include "rdt3.h"

#pragma comment(lib, "ws2_32.lib")  //加载 ws2_32.dll
#pragma comment(lib, "winmm.lib")

using namespace std;
#define min(a, b) a>b?b:a
#define max(a, b) a>b?a:b
#define PORT 7879
static int addrLen;
double MAX_TIME = CLOCKS_PER_SEC / 4;
static u_long fileLen;
string ADDRSRV;
SOCKADDR_IN addrSrv;
static const u_long windowSize = 8 * MSS;
static u_int base = 0;//握手阶段确定的初始序列号
static u_int nextSeqNum = base;
static mutex mutexLock;

static clock_t start;
static bool stopTimer = true;

static u_long cwnd = MSS;
static u_long ssthresh = 6 * MSS;
static int dupACKCount = 0;
static u_long window;
//sender缓冲区
static u_long lastSendByte = 0, lastAckByte = 0;
static Packet sendPkts[50]{};

//RENO

static int RENO_STAGE = START_UP;

// new RENO
static bool fastResend = false;
static bool timeOutResend = false;

bool connectToServer(SOCKET &socket, SOCKADDR_IN &addr) {
    int len = sizeof(addr);

    PacketHead head;
    head.flag |= SYN;
    head.seq = base;
    head.checkSum = CheckPacketSum((u_short *) &head, sizeof(head));

    char *buffer = new char[sizeof(head)];
    memcpy(buffer, &head, sizeof(head));
    sendto(socket, buffer, sizeof(head), 0, (sockaddr *) &addr, len);
    ShowPacket((Packet *) &head);
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
    ShowPacket((Packet *) &head);
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
    ShowPacket((Packet *) &head);

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
    cout << "[ACK_SEND]三次握手成功" << endl;
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

    ShowPacket((Packet *) &closeHead);
    if (sendto(socket, buffer, sizeof(PacketHead), 0, (SOCKADDR *) &addr, addrLen) != SOCKET_ERROR)
        cout << "[FIN_SEND]第一次挥手成功" << endl;
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
    ShowPacket((Packet *) buffer);
    if ((((PacketHead *) buffer)->flag & ACK) && (CheckPacketSum((u_short *) buffer, sizeof(PacketHead) == 0))) {
        cout << "[ACK_RECV]第二次挥手成功，客户端已经断开" << endl;
    } else {
        return false;
    }

    u_long imode = 0;
    ioctlsocket(socket, FIONBIO, &imode);//阻塞
    recvfrom(socket, buffer, sizeof(PacketHead), 0, (SOCKADDR *) &addr, &addrLen);
    memcpy(&closeHead, buffer, sizeof(PacketHead));
    ShowPacket((Packet *) buffer);
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
    ShowPacket((Packet *) &closeHead);
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

Packet makePacket(u_int seq, char *data, int len) {
    Packet pkt;
    pkt.head.seq = seq;
    pkt.head.bufSize = len;
    memcpy(pkt.data, data, len);
    pkt.head.checkSum = CheckPacketSum((u_short *) &pkt, sizeof(Packet));
    return pkt;
}

u_int waitingNum(u_int nextSeq) {
    if (nextSeq >= base)
        return nextSeq - base;
    return nextSeq + MAX_SEQ - base;
}

DWORD WINAPI ACKHandler(LPVOID param) {
    SOCKET *clientSock = (SOCKET *) param;
    char recvBuffer[sizeof(Packet)];
    Packet recvPacket;

    while (true) {
        if (recvfrom(*clientSock, recvBuffer, sizeof(Packet), 0, (SOCKADDR *) &addrSrv, &addrLen) > 0) {
            memcpy(&recvPacket, recvBuffer, sizeof(Packet));
            mutexLock.lock();
            if (CheckPacketSum((u_short *) &recvPacket, sizeof(Packet)) == 0 && recvPacket.head.flag & ACK) {
                if (base < (recvPacket.head.ack + 1)) {

                    int d = recvPacket.head.ack + 1 - base;
                    for (int i = 0; i < d; i++) {
                        lastAckByte += sendPkts[i].head.bufSize;
                    }
                    for (int i = 0; i < (int) waitingNum(nextSeqNum) - d; i++) {
                        sendPkts[i] = sendPkts[i + d];
                    }
                    string stageName;
                    switch (RENO_STAGE) {
                        case START_UP:
                            cwnd += d * MSS;
                            dupACKCount = 0;
                            if (cwnd >= ssthresh)
                                RENO_STAGE = AVOID;
                            break;
                        case AVOID:
                            cwnd += d * MSS * MSS / cwnd;
                            dupACKCount = 0;
                            break;
                        case RECOVERY:
                            cwnd = ssthresh;
                            dupACKCount = 0;
                            RENO_STAGE = AVOID;
                            break;
                    }
                    window = min(cwnd, windowSize);
                    base = (recvPacket.head.ack + 1) % MAX_SEQ;
                    stageName = getRENOStageName(RENO_STAGE);
#ifdef OUTPUT_LOG
                    cout << "[" << stageName << "]cwnd:" << cwnd << "\twindow:" << window << "\tssthresh:" << ssthresh
                         << endl;
                    cout << "[lastACKByte:" << lastAckByte << "\tlastSendByte:" << lastSendByte
                         << "\tlastWritenByte:"
                         << lastAckByte + window << "]" << endl;
#endif
                } else {
                    //duplicate ACK
                    dupACKCount++;
                    if (RENO_STAGE == START_UP || RENO_STAGE == AVOID) {
                        if (dupACKCount == 3) {
                            ssthresh = cwnd / 2;
                            cwnd = ssthresh + 3 * MSS;
                            RENO_STAGE = RECOVERY;
                            //retransmit missing segment
                            fastResend = true;
#ifdef OUTPUT_LOG
                            cout << "ACK DUP 3 times!Retransmit the missing packet" << endl;
#endif
                            /* int reSendSeq = recvPacket.head.ack + 1;
                             memcpy(recvBuffer, &sendPkts[waitingNum(reSendSeq)], sizeof(Packet));
                             sendto(*clientSock, recvBuffer, sizeof(Packet), 0, (SOCKADDR *) &addrSrv, addrLen);*/
                        }
                    } else {
                        cwnd += MSS;
                    }

                    string stageName = getRENOStageName(RENO_STAGE);
#ifdef OUTPUT_LOG
                    cout << "[" << stageName << "]cwnd:" << cwnd << "\twindow:" << window << "\tssthresh:" << ssthresh
                         << endl;
                    cout << "[lastACKByte:" << lastAckByte << "\tlastSendByte:" << lastSendByte
                         << "\tlastWritenByte:"
                         << lastAckByte + window << "]" << endl;
#endif
                }

                if (base == nextSeqNum)
                    stopTimer = true;
                else {
                    start = clock();
                    stopTimer = false;
                }
            }
            mutexLock.unlock();
            if (lastAckByte == fileLen)
                return 0;
        }
    }
}

DWORD WINAPI TIMEOUTHandler(LPVOID param) {
    while (true) {

        if (lastAckByte == fileLen)
            return 0;
        if (!stopTimer) {
            if (clock() - start > MAX_TIME) {
                timeOutResend = true;
#ifdef OUTPUT_LOG
                cout << "[time out!]Begin Resend" << endl;
#endif
            }
        }
    }
}

void sendFSM(u_long len, char *fileBuffer, SOCKET &socket, SOCKADDR_IN &addr) {

    int packetDataLen;

    char *data_buffer = new char[sizeof(Packet)], *pkt_buffer = new char[sizeof(Packet)];
    nextSeqNum = base;
    cout << "本次文件数据长度为" << len << "Bytes" << endl;

    int sumPackets = 0, lossPackets = 0;
    auto nBeginTime = chrono::system_clock::now();
    auto nEndTime = nBeginTime;
    HANDLE ackhandler = CreateThread(nullptr, 0, ACKHandler, LPVOID(&socket), 0, nullptr);
    HANDLE timeouthandler = CreateThread(nullptr, 0, TIMEOUTHandler, nullptr, 0, nullptr);
    string stageName;
    while (true) {

        if (lastAckByte == len) {
            nEndTime = chrono::system_clock::now();
            auto duration = chrono::duration_cast<chrono::microseconds>(nEndTime - nBeginTime);
            double lossRate = double(lossPackets) / sumPackets;
            printf("System use %lf s, and the rate of loss packet is %lf\n",
                   double(duration.count()) * chrono::microseconds::period::num /
                   chrono::microseconds::period::den, lossRate);

            CloseHandle(ackhandler);
            CloseHandle(timeouthandler);
            PacketHead endPacket;
            endPacket.flag |= END;
            endPacket.checkSum = CheckPacketSum((u_short *) &endPacket, sizeof(PacketHead));
            memcpy(pkt_buffer, &endPacket, sizeof(PacketHead));
            sendto(socket, pkt_buffer, sizeof(PacketHead), 0, (SOCKADDR *) &addr, addrLen);

            while (recvfrom(socket, pkt_buffer, sizeof(PacketHead), 0, (SOCKADDR *) &addr, &addrLen) <= 0) {
                if (clock() - start >= MAX_TIME) {
                    start = clock();
                    goto resend;
                }
            }

            if (((PacketHead *) (pkt_buffer))->flag & ACK &&
                CheckPacketSum((u_short *) pkt_buffer, sizeof(PacketHead)) == 0) {
                cout << "文件传输完成" << endl;
                return;
            }

            resend:
            continue;
        }

        if (fastResend || timeOutResend)
            goto GBN;

        mutexLock.lock();
        window = min(cwnd, windowSize);
        if ((lastSendByte < lastAckByte + window) && (lastSendByte < len)) {
            sumPackets++;

            packetDataLen = min(lastAckByte + window - lastSendByte, MSS);
            packetDataLen = min(packetDataLen, len - lastSendByte);
            memcpy(data_buffer, fileBuffer + lastSendByte, packetDataLen);

            sendPkts[nextSeqNum - base] = makePacket(nextSeqNum, data_buffer, packetDataLen);
            memcpy(pkt_buffer, &sendPkts[nextSeqNum - base], sizeof(Packet));

            sendto(socket, pkt_buffer, sizeof(Packet), 0, (SOCKADDR *) &addr, addrLen);

            if (base == nextSeqNum) {
                start = clock();
                stopTimer = false;
            }
            nextSeqNum = (nextSeqNum + 1) % MAX_SEQ;
            lastSendByte += packetDataLen;
        }
        mutexLock.unlock();
        continue;

        GBN:
        mutexLock.lock();
        for (int i = 0; i < nextSeqNum - base; i++) {
            memcpy(pkt_buffer, &sendPkts[i], sizeof(Packet));
            sendto(socket, pkt_buffer, sizeof(Packet), 0, (SOCKADDR *) &addr, addrLen);
        }
        if (timeOutResend) {
            ssthresh = cwnd / 2;
            cwnd = MSS;
            dupACKCount = 0;
            RENO_STAGE = START_UP;
        }
        timeOutResend = fastResend = false;
        mutexLock.unlock();
        start = clock();
        stopTimer = false;
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

    addrSrv.sin_family = AF_INET;
    addrSrv.sin_port = htons(PORT);
    addrSrv.sin_addr.S_un.S_addr = inet_addr(ADDRSRV.c_str());
    addrLen = sizeof(addrSrv);
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
    fileLen = infile.tellg();
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
    cout << "文件传输完成" << endl;
    return 1;
}