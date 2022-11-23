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
static SOCKADDR_IN addrSrv;
static int addrLen = sizeof(addrSrv);
#define PORT 7879
double MAX_TIME = CLOCKS_PER_SEC;

string ADDRSRV;
static const int windowSize = 16;
static unsigned int base = 0;//���ֽ׶�ȷ���ĳ�ʼ���к�
static unsigned int nextSeqNum = 0;
static Packet sendPkt[windowSize]{};
static int sendIndex = 0, recvIndex = 0;
static bool stopTimer = false;
static clock_t start;
u_int waitingNum(u_int nextSeq) {
    if (nextSeq >= base)
        return nextSeq - base;
    return nextSeq + MAX_SEQ - base;
}

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
    cout << "[SYN_SEND]��һ�����ֳɹ�" << endl;

    clock_t start = clock(); //��ʼ��ʱ
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
        cout << "[ACK_RECV]�ڶ������ֳɹ�" << endl;
    } else {
        return false;
    }

    //��������������
    head.flag = 0;
    head.flag |= ACK;
    head.checkSum = 0;
    head.checkSum = (CheckPacketSum((u_short *) &head, sizeof(head)));
    memcpy(buffer, &head, sizeof(head));
    sendto(socket, buffer, sizeof(head), 0, (sockaddr *) &addr, len);
    ShowPacket((Packet*)&head);

    //�ȴ�����MAX_TIME�����û���յ���Ϣ˵��ACKû�ж���
    start = clock();
    while (clock() - start <= 2 * MAX_TIME) {
        if (recvfrom(socket, buffer, sizeof(PacketHead), 0, (SOCKADDR *) &addr, &len) <= 0)
            continue;
        //˵�����ACK����
        memcpy(buffer, &head, sizeof(head));
        sendto(socket, buffer, sizeof(head), 0, (sockaddr *) &addr, len);
        start = clock();
    }
    cout<<"[ACK_SEND]�������ֳɹ�"<<endl;
    cout << "[CONNECTED]�ɹ���������������ӣ�׼����������" << endl;
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
        cout<<"[FIN_SEND]��һ�λ��ֳɹ�"<<endl;
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
        cout << "[ACK_RECV]�ڶ��λ��ֳɹ����ͻ����Ѿ��Ͽ�" << endl;
    } else {
        return false;
    }

    u_long imode = 0;
    ioctlsocket(socket, FIONBIO, &imode);//����
    recvfrom(socket, buffer, sizeof(PacketHead), 0, (SOCKADDR *) &addr, &addrLen);
    memcpy(&closeHead,buffer,sizeof(PacketHead));
    ShowPacket((Packet*)buffer);
    if ((((PacketHead *) buffer)->flag & FIN) && (CheckPacketSum((u_short *) buffer, sizeof(PacketHead) == 0))) {
        cout << "[FIN_RECV]�����λ��ֳɹ����������Ͽ�" << endl;
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
        //˵�����ACK����
        memcpy(buffer, &closeHead, sizeof(PacketHead));
        sendto(socket, buffer, sizeof(PacketHead), 0, (sockaddr *) &addr, addrLen);
        start = clock();
    }

    cout << "[ACK_SEND]���Ĵλ��ֳɹ��������ѹر�" << endl;
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

bool inWindows(u_int seq) {
    if (seq >= base && seq < base + windowSize)
        return true;
    if (seq < base && seq < ((base + windowSize) % MAX_SEQ))
        return true;

    return false;
}

DWORD WINAPI ACKHandler(LPVOID param){
    SOCKET* clientSock = (SOCKET*)param;
    char recvBuffer[sizeof(Packet)];
    Packet recvPacket;
    while(true){
        if(recvfrom(*clientSock,recvBuffer, sizeof(Packet),0,(SOCKADDR*)&addrSrv,&addrLen)>0){
            memcpy(&recvPacket,recvBuffer, sizeof(Packet));
            if(CheckPacketSum((u_short*)&recvPacket, sizeof(Packet))==0&&recvPacket.head.flag&ACK){
                if (base < (recvPacket.head.ack + 1)) {
                    int d = recvPacket.head.ack + 1 - base;
                    for (int i = 0; i < (int) waitingNum(nextSeqNum) - d; i++) {
                        sendPkt[i] = sendPkt[i + d];
                    }
                    recvIndex+=d;
                    base = (max((recvPacket.head.ack + 1), base)) % MAX_SEQ;
                    cout << "base:" << base << " nextSeq:" << nextSeqNum << " endWindow:" << base + windowSize << endl;
                }
                if (base == nextSeqNum)
                    stopTimer = true;
                else {
                    start = clock();
                    stopTimer = false;
                }
            }
        }
    }
}

void sendFSM(u_long len, char *fileBuffer, SOCKET &socket, SOCKADDR_IN &addr) {

    int packetNum = int(len / MAX_DATA_SIZE) + (len % MAX_DATA_SIZE ? 1 : 0);

    int packetDataLen;
    int addrLen = sizeof(addr);
    char *data_buffer = new char[packetDataLen], *pkt_buffer = new char[sizeof(Packet)];
    Packet recvPkt;
    nextSeqNum = base;
    cout << "�����ļ����ݳ���Ϊ" << len << "Bytes,��Ҫ����" << packetNum << "�����ݰ�" << endl;
    //HANDLE ackhandler = CreateThread(NULL,0,ACKHandler,LPVOID(&socket),0,NULL);
    while (true) {
        if (recvIndex == packetNum) {
            //CloseHandle(ackhandler);
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
                cout << "�ļ��������" << endl;
                return;
            }

            resend:
            continue;
        }

        packetDataLen = min(MAX_DATA_SIZE, len - sendIndex * MAX_DATA_SIZE);

        if (inWindows(nextSeqNum) && sendIndex < packetNum) {

            memcpy(data_buffer, fileBuffer + sendIndex * MAX_DATA_SIZE, packetDataLen);
            sendPkt[(int) waitingNum(nextSeqNum)] = makePacket(nextSeqNum, data_buffer, packetDataLen);
            memcpy(pkt_buffer, &sendPkt[(int) waitingNum(nextSeqNum)], sizeof(Packet));
            sendto(socket, pkt_buffer, sizeof(Packet), 0, (SOCKADDR *) &addr, addrLen);

            if (base == nextSeqNum) {
                start = clock();
                stopTimer = false;
            }
            nextSeqNum = (nextSeqNum + 1) % MAX_SEQ;
            sendIndex++;
            cout << sendIndex << "�����ݰ��Ѿ�����" << endl;
        }

        while (recvfrom(socket, pkt_buffer, sizeof(Packet), 0, (SOCKADDR *) &addr, &addrLen) > 0) {
            memcpy(&recvPkt, pkt_buffer, sizeof(Packet));
            //corrupt
            if (CheckPacketSum((u_short *) &recvPkt, sizeof(Packet)) != 0 || !(recvPkt.head.flag & ACK))
                goto time_out;
            //not corrupt
            if (base < (recvPkt.head.ack + 1)) {
                int d = recvPkt.head.ack + 1 - base;
                for (int i = 0; i < (int) waitingNum(nextSeqNum) - d; i++) {
                    sendPkt[i] = sendPkt[i + d];
                }
                recvIndex+=d;
                base = (max((recvPkt.head.ack + 1), base)) % MAX_SEQ;
                cout << "base:" << base << " nextSeq:" << nextSeqNum << " endWindow:" << base + windowSize << endl;
            }
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
            for (int i = 0; i < (int) waitingNum(nextSeqNum); i++) {
                memcpy(pkt_buffer, &sendPkt[i], sizeof(Packet));
                sendto(socket, pkt_buffer, sizeof(Packet), 0, (SOCKADDR *) &addr, addrLen);
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
        cout << "[SYSTEM]����DLLʧ��" << endl;
        return -1;
    }
    SOCKET sockClient = socket(AF_INET, SOCK_DGRAM, 0);

    u_long imode = 1;
    ioctlsocket(sockClient, FIONBIO, &imode);//������

    cout << "[NOT CONNECTED]����������������ĵ�ַ" << endl;
    cin >> ADDRSRV;

    addrSrv.sin_family = AF_INET;
    addrSrv.sin_port = htons(PORT);
    addrSrv.sin_addr.S_un.S_addr = inet_addr(ADDRSRV.c_str());

    if (!connectToServer(sockClient, addrSrv)) {
        cout << "[ERROR]����ʧ��" << endl;
        return 0;
    }

    string filename;
    cout << "[SYSTEM]��������Ҫ������ļ���" << endl;
    cin >> filename;

    ifstream infile(filename, ifstream::binary);

    if (!infile.is_open()) {
        cout << "[ERROR]�޷����ļ�" << endl;
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
    cout << "[SYSTEM]��ʼ����" << endl;

    sendFSM(fileLen, fileBuffer, sockClient, addrSrv);

    if (!disConnect(sockClient, addrSrv)) {
        cout << "[ERROR]�Ͽ�ʧ��" << endl;
        return 0;
    }
    cout<<"�ļ��������"<<endl;
    return 1;
}