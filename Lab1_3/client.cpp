#include <cstdio>
#include <WinSock2.h>
#include <windows.h>
#include <iostream>
#include <thread>

#pragma comment(lib, "ws2_32.lib")  //加载 ws2_32.dll
#pragma comment(lib, "winmm.lib")

#define PORT 7878
#define BUF_SIZE 512
#define NAME_SIZE 20
#define TYPE_SIZE 10
#define HEAD_SIZE 2*NAME_SIZE+TYPE_SIZE
#define MESSAGE_SIZE HEAD_SIZE+BUF_SIZE

using namespace std;
static string ADDRSRV;
static char userName[NAME_SIZE];

void printSysTime() {
    SYSTEMTIME sysTime;
    GetLocalTime(&sysTime);
    printf("[%4d/%02d/%02d %02d:%02d:%02d.%03d]", sysTime.wYear, sysTime.wMonth, sysTime.wDay,
           sysTime.wHour, sysTime.wMinute, sysTime.wSecond, sysTime.wMilliseconds);
}

DWORD WINAPI handlerSend(LPVOID lparam) {
    SOCKET *socket = (SOCKET *) lparam;

    char type[TYPE_SIZE];
    char fromUser[NAME_SIZE], toUser[NAME_SIZE];
    char buf[BUF_SIZE];
    char message[MESSAGE_SIZE];
    char head[HEAD_SIZE];
    memset(head, 0, HEAD_SIZE);
    memset(type, 0, TYPE_SIZE);
    memset(fromUser, 0, NAME_SIZE);
    memset(toUser, 0, NAME_SIZE);
    memset(message, 0, MESSAGE_SIZE);
    memset(buf, 0, BUF_SIZE);

    strcpy(fromUser, userName);

    while (1) {
        cin.getline(buf,512);
        //设计head头信息

        //如果输入为@，则是向另一个用户私发信息
        if (strcmp(buf, "@") == 0) {
            cout << "请输入你想私聊的用户名:";
            cin >> toUser;
            cin.ignore();
            //TYPE是“PRI”是私聊信息
            strcpy(type, "PRI");
            strcpy(head, type);
            for (int j = 0; j < NAME_SIZE; j++)
                head[j + TYPE_SIZE] = fromUser[j];
            for (int j = 0; j < NAME_SIZE; j++)
                head[j + TYPE_SIZE + NAME_SIZE] = toUser[j];

            cin.getline(buf,512);

        } else {
            //TYPE为“PUB”是群发信息，不需要toUser
            strcpy(type, "PUB");
            strcpy(head, type);
            for (int j = 0; j < NAME_SIZE; j++)
                head[j + TYPE_SIZE] = fromUser[j];
        }

        //将HEAD和BUF内容移动到MESSAGE的对应位置
        for (int j = 0; j < HEAD_SIZE; j++)
            message[j] = head[j];
        for (int j = 0; j < BUF_SIZE; j++)
            message[j + HEAD_SIZE] = buf[j];

        int len = send(*socket, message, MESSAGE_SIZE, 0);
        //如果输入quit，直接退出聊天室
        if (strcmp(buf, "quit") == 0) {
            printSysTime();
            cout << "[System Info]";
            cout << "您已退出聊天室" << endl;
            getchar();
            return 0;
        }
        if (len > 0) {
            printSysTime();
            cout << "发送成功" << endl;
        }

        memset(head, 0, HEAD_SIZE);
        memset(type, 0, TYPE_SIZE);
        memset(toUser, 0, NAME_SIZE);
        memset(message, 0, MESSAGE_SIZE);
        memset(buf, 0, BUF_SIZE);
    }
}

DWORD WINAPI handlerRec(LPVOID lparam) {
    SOCKET *socket = (SOCKET *) lparam;

    char type[TYPE_SIZE];
    char fromUser[NAME_SIZE];
    char buf[BUF_SIZE];
    char message[MESSAGE_SIZE];
    memset(type, 0, TYPE_SIZE);
    memset(fromUser, 0, NAME_SIZE);
    memset(message, 0, MESSAGE_SIZE);
    memset(buf, 0, BUF_SIZE);

    while (1) {
        recv(*socket, message, MESSAGE_SIZE, 0);
        for (int j = 0; j < TYPE_SIZE; j++)
            type[j] = message[j];

        for (int j = 0; j < BUF_SIZE; j++)
            buf[j] = message[j + HEAD_SIZE];
        if (strcmp(type, "SYS") == 0) {
            //系统发出的消息,结束对服务器服务，直接结束线程
            if (strcmp(buf, "exit") == 0) {
                return 0;
            }
            printf("[系统通知]%s\n",buf);
            continue;
        }
        //设置user名字字段
        for(int j=0;j<NAME_SIZE;j++)
            fromUser[j]=message[j+TYPE_SIZE];

        if (strcmp(buf, "quit") == 0) {
            printSysTime();
            printf("用户[%s]已经退出聊天室\n", fromUser);
            continue;
        }

        if(strcmp(type,"PRI")==0){
            printSysTime();
            printf("用户[%s](私聊):%s\n",fromUser,buf);
        }
        else {
            printSysTime();
            printf("用户[%s]:%s\n", fromUser, buf);
        }
        memset(message, 0, MESSAGE_SIZE);
        memset(type, 0, TYPE_SIZE);
        memset(fromUser, 0, NAME_SIZE);
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


    cout << "[NOT CONNECTED]请输入聊天服务器的地址" << endl;
    cin >> ADDRSRV;

    SOCKADDR_IN addrSrv;
    addrSrv.sin_family = AF_INET;
    addrSrv.sin_port = htons(PORT);
    addrSrv.sin_addr.S_un.S_addr = inet_addr(ADDRSRV.c_str());

    if (connect(sockClient, (SOCKADDR *) &addrSrv, sizeof(SOCKADDR)) != 0) {
        //TCP连接失败
        cout << "连接失败" << endl;
        return -1;
    }
    cout << "[CONNECTED]SOCKET连接成功" << endl;
    //将用户名给客户端保存
    while(1){
        cout << "[CONNECTED]请输入您的用户名" << endl;
        cin >> userName;
        send(sockClient,userName,NAME_SIZE,0);

        char t[10];
        recv(sockClient,t,10,0);
        if(strcmp(t,"TRUE")==0){
            break;
        }
        cout<<"[CONNECTED]该用户名已存在，请重新输入"<<endl;
    }
    cout<<"[CONNECTED]已连接到服务器"<<endl;
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