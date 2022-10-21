#include <cstdio>
#include <WinSock2.h>
#include <windows.h>
#include <iostream>
#include <thread>
#include <map>

#pragma comment(lib, "ws2_32.lib")  //加载 ws2_32.dll

#define PORT 7878
#define NAME_SIZE 20
#define TYPE_SIZE 10
#define HEAD_SIZE 2*NAME_SIZE+TYPE_SIZE
#define BUF_SIZE 512
#define MESSAGE_SIZE HEAD_SIZE+BUF_SIZE

#define ADDRSRV "127.0.0.1"
#define MAXUSER 50
using namespace std;

static map<string,int>userNameMap;
static SOCKET sockConnects[MAXUSER];

void printSysTime(){
    SYSTEMTIME sysTime;
    GetLocalTime(&sysTime);
    printf("[%4d/%02d/%02d %02d:%02d:%02d.%03d]", sysTime.wYear, sysTime.wMonth, sysTime.wDay,
           sysTime.wHour, sysTime.wMinute, sysTime.wSecond, sysTime.wMilliseconds);
}

void transfer2AllUser(const char*message){
    for(int j=0;j<MAXUSER;j++){
        if(sockConnects[j]!= SOCKET_ERROR)
            send(sockConnects[j],message,MESSAGE_SIZE,0);
    }
}

void transfer2OneUser(int i,const char*message){
    send(sockConnects[i],message,MESSAGE_SIZE,0);
}

DWORD WINAPI handlerTransfer(LPVOID lparam) {
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

    int i = 0;
    for(int j=0;j<MAXUSER;j++,i++)
        if(&sockConnects[j]==socket)
            break;

    while (1) {
        recv(*socket, message, MESSAGE_SIZE, 0);

        for(int j=0;j<BUF_SIZE;j++)
            buf[j]=message[HEAD_SIZE+j];
        for(int j=0;j<TYPE_SIZE;j++)
            type[j]=message[j];
        for(int j=0;j<NAME_SIZE;j++){
            fromUser[j]=message[j+TYPE_SIZE];
            toUser[j]=message[j+TYPE_SIZE+NAME_SIZE];
        }

        if (strcmp(buf, "quit") == 0) {
            printSysTime();
            printf("用户[%s]已经退出聊天室\n", fromUser);
            transfer2AllUser(message);
            char temMessage[MESSAGE_SIZE]="SYS";
            char temBuf[BUF_SIZE]="exit";

            for(int j=0;j<MESSAGE_SIZE;j++)
                temMessage[j+HEAD_SIZE]=temBuf[j];

            userNameMap.erase(string(fromUser));
            send(sockConnects[i],temMessage,MESSAGE_SIZE,0);
            closesocket(sockConnects[i]);
            return 0;
        }
        if (strlen(buf) > 0) {
            if (strcmp(type, "PRI") == 0) {
                printSysTime();
                printf("用户[%s]私聊[%s]:%s\n", fromUser, toUser, buf);
                transfer2OneUser(userNameMap[string(toUser)],message);
            } else {
                printSysTime();
                printf("[用户%s]:%s\n", fromUser, buf);
                transfer2AllUser(message);
            }
        }
        memset(message, 0, BUF_SIZE);
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
        cout << "-----服务器进入监听状态-----" << endl;
    } else {
        cout << "监听失败" << endl;
        return -1;
    }

    for(int j=0;j<MAXUSER;j++)
        sockConnects[j]= SOCKET_ERROR;

    while(1){
        int index=0;
        for(;index<MAXUSER;index++) {
            if (sockConnects[index] == SOCKET_ERROR)
                break;
        }
        if(index==MAXUSER){
            cout<<"目前服务器已经到底最大连接人数"<<endl;
            continue;
        }
        SOCKADDR_IN addrClient;
        int lenAddr = sizeof(SOCKADDR);
        sockConnects[index] =accept(sockServer, (SOCKADDR *) &addrClient, &(lenAddr));
        if (sockConnects[index] == SOCKET_ERROR) {
            cout << "连接失败" << endl;
            sockConnects[index]= SOCKET_ERROR;
            continue;
        }
        while(1) {
            char name[NAME_SIZE];
            string t("TRUE");
            recv(sockConnects[index], name, NAME_SIZE, 0);
            if(userNameMap.find(string(name))==userNameMap.end()){
                userNameMap.insert(pair<string, int>(string(name), index));
                send(sockConnects[index],t.c_str(),10,0);
                cout << "新客户端连接成功,它的编号是:" << index << "用户名为:" << name << " IP地址:" << inet_ntoa(addrClient.sin_addr) << endl;

                char mess[MESSAGE_SIZE]="SYS";
                char b[BUF_SIZE]="新用户了加入聊天室,用户名为:";
                strcat(b,name);
                for(int j=0;j<BUF_SIZE;j++)
                    mess[j+HEAD_SIZE]=b[j];
                transfer2AllUser(mess);
                break;
            }
            else{
                t="FALSE";
                send(sockConnects[index],t.c_str(),10,0);
            }
        }
        CloseHandle(CreateThread(NULL,NULL,handlerTransfer,(LPVOID)&sockConnects[index],0,NULL));
    }

    /*SOCKADDR_IN addrClient1;
    int lenAddr = sizeof(SOCKADDR);
    sockConnects[0] = accept(sockServer, (SOCKADDR *) &addrClient1, &(lenAddr));
    if (sockConnects[0] == SOCKET_ERROR) {
        cout << "连接失败" << endl;
        return -1;
    }
    cout << "客户端1连接成功,它的IP是:" << inet_ntoa(addrClient1.sin_addr) << endl;

    SOCKADDR_IN addrClient2;
    lenAddr = sizeof(SOCKADDR);
    sockConnects[1] = accept(sockServer, (SOCKADDR *) &addrClient2, &(lenAddr));
    if (sockConnects[1] == SOCKET_ERROR) {
        cout << "连接失败" << endl;
        return -1;
    }
    cout << "客户端2连接成功,它的IP是:" << inet_ntoa(addrClient2.sin_addr) << endl;

    HANDLE hthread[2];
    hthread[0] = CreateThread(NULL, NULL, handlerTransfer, (LPVOID) &sockConnects[0], 0, NULL);
    hthread[1] = CreateThread(NULL, NULL, handlerTransfer, (LPVOID) &sockConnects[1], 0, NULL);
    WaitForMultipleObjects(2, hthread, TRUE, INFINITE);
    CloseHandle(hthread[0]);
    CloseHandle(hthread[1]);

    closesocket(sockConnects[0]);
    closesocket(sockConnects[1]);
    closesocket(sockServer);*/
    WSACleanup();
    getchar();
    return 0;
}