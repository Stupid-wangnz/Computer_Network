#include <cstdio>
#include <WinSock2.h>
#include <windows.h>
#include <iostream>
#include <thread>

#pragma comment(lib, "ws2_32.lib")  //加载 ws2_32.dll

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
            printf("%4d/%02d/%02d %02d:%02d:%02d.%03d 星期%1d\n", sysTime.wYear, sysTime.wMonth, sysTime.wDay,
                   sysTime.wHour, sysTime.wMinute, sysTime.wSecond, sysTime.wMilliseconds, sysTime.wDayOfWeek);
            cout << "您已退出聊天室" << endl;
            return 0;
        }
        if (len > 0) {
            SYSTEMTIME sysTime;
            GetLocalTime(&sysTime);
            printf("%4d/%02d/%02d %02d:%02d:%02d.%03d 星期%1d\n", sysTime.wYear, sysTime.wMonth, sysTime.wDay,
                   sysTime.wHour, sysTime.wMinute, sysTime.wSecond, sysTime.wMilliseconds, sysTime.wDayOfWeek);
            cout << "--------------发送消息成功--------------" << endl;
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
            printf("%4d/%02d/%02d %02d:%02d:%02d.%03d 星期%1d\n", sysTime.wYear, sysTime.wMonth, sysTime.wDay,
                   sysTime.wHour, sysTime.wMinute, sysTime.wSecond, sysTime.wMilliseconds, sysTime.wDayOfWeek);
            cout << "对方已退出聊天室" << endl;
            cout << "本次聊天结束，输入quit退出" << endl;
            return 0;
        }
        if (len > 0) {
            SYSTEMTIME sysTime;
            GetLocalTime(&sysTime);
            printf("%4d/%02d/%02d %02d:%02d:%02d.%03d 星期%1d\n", sysTime.wYear, sysTime.wMonth, sysTime.wDay,
                   sysTime.wHour, sysTime.wMinute, sysTime.wSecond, sysTime.wMilliseconds, sysTime.wDayOfWeek);
            printf("对方用户:%s\n", buf);
        }
        memset(buf, 0, BUF_SIZE);
    }
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        //加载失败
        cout << "加载DLL失败" << endl;
        return -1;
    }
    SOCKET sockServer = socket(AF_INET, SOCK_STREAM, 0);

    SOCKADDR_IN addrSrv;
    addrSrv.sin_family = AF_INET;
    addrSrv.sin_port = htons(PORT);
    addrSrv.sin_addr.S_un.S_addr = inet_addr(ADDRSRV);
    bind(sockServer, (SOCKADDR *) &addrSrv, sizeof(SOCKADDR));

    if (listen(sockServer, 10) == 0) {
        cout << "服务器进入监听状态" << endl;
    } else {
        cout << "监听失败" << endl;
        return -1;
    }

    SOCKADDR_IN addrClient;
    int lenAddr = sizeof(SOCKADDR);
    SOCKET sockConnect = accept(sockServer, (SOCKADDR *) &addrClient, &(lenAddr));
    if (sockConnect == SOCKET_ERROR) {
        cout << "连接失败" << endl;
        return -1;
    }
    cout << "连接成功,对方的IP是:" << inet_ntoa(addrClient.sin_addr) << endl;

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