#include <cstdio>
#include <WinSock2.h>
#include <windows.h>
#include <iostream>
#include <thread>

#pragma comment(lib, "ws2_32.lib")  //���� ws2_32.dll

#define PORT 7878
#define BUF_SIZE 512
#define ADDRSRV "127.0.0.1"
int CLIENTNUM=2;
using namespace std;

DWORD WINAPI handlerTransfer(LPVOID lparam) {
    SOCKET *socket = (SOCKET *) lparam;
    char buf[BUF_SIZE];
    memset(buf, 0, BUF_SIZE);
    while (1) {
        for (int i = 0; i < CLIENTNUM; i++) {
            int len = recv(socket[i], buf, BUF_SIZE, 0);
            if (strcmp(buf, "quit") == 0) {
                SYSTEMTIME sysTime;
                GetLocalTime(&sysTime);
                printf("%4d/%02d/%02d %02d:%02d:%02d.%03d ����%1d\n", sysTime.wYear, sysTime.wMonth, sysTime.wDay,
                       sysTime.wHour, sysTime.wMinute, sysTime.wSecond, sysTime.wMilliseconds, sysTime.wDayOfWeek);
                printf("�û�%d�Ѿ��˳�������",i);
                send(socket[(i+1)%2],buf, strlen(buf),0);
            }
            if (len > 0) {
                SYSTEMTIME sysTime;
                GetLocalTime(&sysTime);
                printf("%4d/%02d/%02d %02d:%02d:%02d.%03d ����%1d\n", sysTime.wYear, sysTime.wMonth, sysTime.wDay,
                       sysTime.wHour, sysTime.wMinute, sysTime.wSecond, sysTime.wMilliseconds, sysTime.wDayOfWeek);
                printf("�û�%d:%s\n", i,buf);
                send(socket[(i+1)%2],buf, strlen(buf),0);
            }
            memset(buf, 0, BUF_SIZE);
        }
    }
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        //����ʧ��
        cout << "����DLLʧ��" << endl;
        return -1;
    }
    SOCKET sockServer = socket(AF_INET, SOCK_STREAM, 0);

    SOCKADDR_IN addrSrv;
    addrSrv.sin_family = AF_INET;
    addrSrv.sin_port = htons(PORT);
    addrSrv.sin_addr.S_un.S_addr = inet_addr(ADDRSRV);
    bind(sockServer, (SOCKADDR *) &addrSrv, sizeof(SOCKADDR));

    if (listen(sockServer, 10) == 0) {
        cout << "�������������״̬" << endl;
    } else {
        cout << "����ʧ��" << endl;
        return -1;
    }

    SOCKET sockConnects[2];

    SOCKADDR_IN addrClient1;
    int lenAddr = sizeof(SOCKADDR);
    sockConnects[0] = accept(sockServer, (SOCKADDR *) &addrClient1, &(lenAddr));
    if (sockConnects[0]== SOCKET_ERROR) {
        cout << "����ʧ��" << endl;
        getchar();
        return -1;
    }
    cout << "�ͻ���1���ӳɹ�,����IP��:" << inet_ntoa(addrClient1.sin_addr) << endl;

    SOCKADDR_IN addrClient2;
    lenAddr = sizeof(SOCKADDR);
    sockConnects[1] = accept(sockServer, (SOCKADDR *) &addrClient2, &(lenAddr));
    if (sockConnects[1] == SOCKET_ERROR) {
        cout << "����ʧ��" << endl;
        getchar();
        return -1;
    }
    cout << "�ͻ���2���ӳɹ�,����IP��:" << inet_ntoa(addrClient2.sin_addr) << endl;

    HANDLE hthread;
    hthread = CreateThread(NULL, NULL, handlerTransfer, (LPVOID) sockConnects, 0, NULL);
    WaitForMultipleObjects(1, &hthread, TRUE, INFINITE);
    CloseHandle(hthread);
    
    closesocket(sockConnects[0]);
    closesocket(sockConnects[1]);
    closesocket(sockServer);
    WSACleanup();
    getchar();
    return 0;
}