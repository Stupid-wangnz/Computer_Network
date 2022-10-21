#include <cstdio>
#include <WinSock2.h>
#include <windows.h>
#include <iostream>
#include <thread>

#pragma comment(lib, "ws2_32.lib")  //���� ws2_32.dll

#define PORT 7878
#define BUF_SIZE 512
#define ADDRSRV "127.0.0.1"
using namespace std;

DWORD WINAPI handlerSend(LPVOID lparam) {
    SOCKET *socket = (SOCKET *) lparam;
    char buf[BUF_SIZE];
    memset(buf, 0, BUF_SIZE);
    while (1) {
        cin >> buf;
        int len = send(*socket, buf, strlen(buf), 0);
        if (strcmp(buf, "quit") == 0) {
            SYSTEMTIME sysTime;
            GetLocalTime(&sysTime);
            printf("%4d/%02d/%02d %02d:%02d:%02d.%03d ����%1d\n", sysTime.wYear, sysTime.wMonth, sysTime.wDay,
                   sysTime.wHour, sysTime.wMinute, sysTime.wSecond, sysTime.wMilliseconds, sysTime.wDayOfWeek);
            cout << "�����˳�������" << endl;
            return 0;
        }
        if (len > 0) {
            SYSTEMTIME sysTime;
            GetLocalTime(&sysTime);
            printf("%4d/%02d/%02d %02d:%02d:%02d.%03d ����%1d\n", sysTime.wYear, sysTime.wMonth, sysTime.wDay,
                   sysTime.wHour, sysTime.wMinute, sysTime.wSecond, sysTime.wMilliseconds, sysTime.wDayOfWeek);
            cout << "--------------������Ϣ�ɹ�--------------" << endl;
        }
        memset(buf, 0, BUF_SIZE);
    }
}

DWORD WINAPI handlerRecv(LPVOID lparam) {
    SOCKET *socket = (SOCKET *) lparam;
    char buf[BUF_SIZE];
    memset(buf, 0, BUF_SIZE);
    while (1) {
        int len = recv(*socket, buf, BUF_SIZE, 0);
        if (strcmp(buf, "quit") == 0) {
            SYSTEMTIME sysTime;
            GetLocalTime(&sysTime);
            printf("%4d/%02d/%02d %02d:%02d:%02d.%03d ����%1d\n", sysTime.wYear, sysTime.wMonth, sysTime.wDay,
                   sysTime.wHour, sysTime.wMinute, sysTime.wSecond, sysTime.wMilliseconds, sysTime.wDayOfWeek);
            cout << "�Է����˳�������" << endl;
            cout << "�����������������quit�˳�" << endl;
            return 0;
        }
        if (len > 0) {
            SYSTEMTIME sysTime;
            GetLocalTime(&sysTime);
            printf("%4d/%02d/%02d %02d:%02d:%02d.%03d ����%1d\n", sysTime.wYear, sysTime.wMonth, sysTime.wDay,
                   sysTime.wHour, sysTime.wMinute, sysTime.wSecond, sysTime.wMilliseconds, sysTime.wDayOfWeek);
            printf("�Է��û�:%s\n", buf);
        }
        memset(buf, 0, BUF_SIZE);
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

    SOCKADDR_IN addrClient;
    int lenAddr = sizeof(SOCKADDR);
    SOCKET sockConnect = accept(sockServer, (SOCKADDR *) &addrClient, &(lenAddr));
    if (sockConnect == SOCKET_ERROR) {
        cout << "����ʧ��" << endl;
        return -1;
    }
    cout << "���ӳɹ�,�Է���IP��:" << inet_ntoa(addrClient.sin_addr) << endl;

    HANDLE hthread[2];
    hthread[0] = CreateThread(NULL, NULL, handlerRecv, (LPVOID) &sockConnect, 0, NULL);
    hthread[1] = CreateThread(NULL, NULL, handlerSend, (LPVOID) &sockConnect, 0, NULL);
    WaitForMultipleObjects(2, hthread, TRUE, INFINITE);
    CloseHandle(hthread[0]);
    CloseHandle(hthread[1]);

    closesocket(sockConnect);
    closesocket(sockServer);
    WSACleanup();

    return 0;
}