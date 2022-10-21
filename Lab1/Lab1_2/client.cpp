#include <cstdio>
#include <WinSock2.h>
#include <windows.h>
#include <iostream>
#include <thread>

#pragma comment(lib, "ws2_32.lib")  //加载 ws2_32.dll
#pragma comment(lib, "winmm.lib")

#define PORT 7878
#define BUF_SIZE 512
using namespace std;
string ADDRSRV;

DWORD WINAPI handlerSend(LPVOID lparam) {
    SOCKET *socket = (SOCKET *) lparam;
    char buf[BUF_SIZE];
    memset(buf, 0, BUF_SIZE);
    while (1) {
        cin >> buf;
        int len = send(*socket, buf, strlen(buf), 0);
        //如果输入quit，直接退出聊天室
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

DWORD WINAPI handlerRec(LPVOID lparam) {
    SOCKET *socket = (SOCKET *) lparam;
    char buf[BUF_SIZE];
    memset(buf, 0, BUF_SIZE);
    string fromUserName(100,'\0');
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
    SOCKET sockClient = socket(AF_INET, SOCK_STREAM, 0);

    cout<<"请输入聊天服务器的地址"<<endl;
    cin>>ADDRSRV;

    SOCKADDR_IN addrSrv;
    addrSrv.sin_family = AF_INET;
    addrSrv.sin_port = htons(PORT);
    addrSrv.sin_addr.S_un.S_addr = inet_addr(ADDRSRV.c_str());

    if (connect(sockClient, (SOCKADDR *) &addrSrv, sizeof(SOCKADDR)) != 0) {
        //TCP连接失败
        cout << "连接失败" << endl;
        return -1;
    }
    cout << "连接成功" << endl;

    HANDLE hthread[2];
    hthread[0] = CreateThread(NULL, 0, handlerRec, (LPVOID) &sockClient, 0, NULL);
    hthread[1] = CreateThread(NULL, 0, handlerSend, (LPVOID) &sockClient, 0, NULL);

    WaitForMultipleObjects(2, hthread, TRUE, INFINITE);
    CloseHandle(hthread[0]);
    CloseHandle(hthread[1]);

    closesocket(sockClient);
    WSACleanup();
    return 0;
}