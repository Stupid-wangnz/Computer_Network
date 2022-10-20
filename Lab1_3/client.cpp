#include <cstdio>
#include <WinSock2.h>
#include <windows.h>
#include <iostream>
#include <thread>

#pragma comment(lib, "ws2_32.lib")  //���� ws2_32.dll
#pragma comment(lib, "winmm.lib")

#define PORT 7878
#define BUF_SIZE 512
#define HEAD_SIZE 50
#define BUF_SIZE 512
#define MESSAGE_SIZE HEAD_SIZE+BUF_SIZE

using namespace std;
string ADDRSRV;
char userName[HEAD_SIZE];

void printSysTime(){
    SYSTEMTIME sysTime;
    GetLocalTime(&sysTime);
    printf("%4d/%02d/%02d %02d:%02d:%02d.%03d ����%1d\n", sysTime.wYear, sysTime.wMonth, sysTime.wDay,
           sysTime.wHour, sysTime.wMinute, sysTime.wSecond, sysTime.wMilliseconds, sysTime.wDayOfWeek);
}

DWORD WINAPI handlerSend(LPVOID lparam) {
    SOCKET *socket = (SOCKET *) lparam;

    char buf[BUF_SIZE];
    char message[MESSAGE_SIZE];
    memset(message,0,MESSAGE_SIZE);
    strcpy(message,userName);
    memset(buf, 0, BUF_SIZE);

    while (1) {
        cin >> buf;
        for(int j=0;j<BUF_SIZE;j++){
            message[j+HEAD_SIZE]=buf[j];
        }

        int len = send(*socket, message, MESSAGE_SIZE, 0);
        //�������quit��ֱ���˳�������
        if (strcmp(buf, "quit") == 0) {
            printSysTime();
            cout << "�����˳�������" << endl;
            return 0;
        }
        if (len > 0) {
            printSysTime();
            cout << "--------------������Ϣ�ɹ�--------------" << endl;
        }
        memset(buf, 0, BUF_SIZE);
    }
}

DWORD WINAPI handlerRec(LPVOID lparam) {
    SOCKET *socket = (SOCKET *) lparam;

    char buf[BUF_SIZE];
    memset(buf, 0, BUF_SIZE);
    char message[MESSAGE_SIZE];
    memset(message,0,MESSAGE_SIZE);
    char head[HEAD_SIZE];
    memset(head,0,HEAD_SIZE);

    while (1) {
        int len = recv(*socket, message, MESSAGE_SIZE, 0);
        for(int j=0;j<HEAD_SIZE;j++)
            head[j]=message[j];
        for(int j=0;j<BUF_SIZE;j++)
            buf[j]=message[j+HEAD_SIZE];

        if(strcmp(head,"SYS")==0){
            //ϵͳ��������Ϣ
            if(strcmp(buf,"exit")==0)
            {
                cout<<"���Ѿ��ͷ������Ͽ�����"<<endl;
                return 0;
            }
        }
        if (strcmp(buf, "quit") == 0) {
            printSysTime();
            printf("�û�[%s]�Ѿ��˳�������",head);
        }
        if (len > 0) {
            printSysTime();
            printf("�û�[%s]:%s\n", head,buf);
        }
        memset(buf, 0, BUF_SIZE);
        memset(head,0,HEAD_SIZE);
    }
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        //����ʧ��
        cout << "����DLLʧ��" << endl;
        return -1;
    }
    SOCKET sockClient = socket(AF_INET, SOCK_STREAM, 0);


    cout<<"����������������ĵ�ַ"<<endl;
    cin>>ADDRSRV;
    cout<<"�����������û���"<<endl;
    cin>>userName;

    SOCKADDR_IN addrSrv;
    addrSrv.sin_family = AF_INET;
    addrSrv.sin_port = htons(PORT);
    addrSrv.sin_addr.S_un.S_addr = inet_addr(ADDRSRV.c_str());

    if (connect(sockClient, (SOCKADDR *) &addrSrv, sizeof(SOCKADDR)) != 0) {
        //TCP����ʧ��
        cout << "����ʧ��" << endl;
        return -1;
    }
    cout << "���ӳɹ�" << endl;

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