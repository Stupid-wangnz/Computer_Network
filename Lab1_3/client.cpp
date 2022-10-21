#include <cstdio>
#include <WinSock2.h>
#include <windows.h>
#include <iostream>
#include <thread>

#pragma comment(lib, "ws2_32.lib")  //���� ws2_32.dll
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
        //���headͷ��Ϣ

        //�������Ϊ@����������һ���û�˽����Ϣ
        if (strcmp(buf, "@") == 0) {
            cout << "����������˽�ĵ��û���:";
            cin >> toUser;
            cin.ignore();
            //TYPE�ǡ�PRI����˽����Ϣ
            strcpy(type, "PRI");
            strcpy(head, type);
            for (int j = 0; j < NAME_SIZE; j++)
                head[j + TYPE_SIZE] = fromUser[j];
            for (int j = 0; j < NAME_SIZE; j++)
                head[j + TYPE_SIZE + NAME_SIZE] = toUser[j];

            cin.getline(buf,512);

        } else {
            //TYPEΪ��PUB����Ⱥ����Ϣ������ҪtoUser
            strcpy(type, "PUB");
            strcpy(head, type);
            for (int j = 0; j < NAME_SIZE; j++)
                head[j + TYPE_SIZE] = fromUser[j];
        }

        //��HEAD��BUF�����ƶ���MESSAGE�Ķ�Ӧλ��
        for (int j = 0; j < HEAD_SIZE; j++)
            message[j] = head[j];
        for (int j = 0; j < BUF_SIZE; j++)
            message[j + HEAD_SIZE] = buf[j];

        int len = send(*socket, message, MESSAGE_SIZE, 0);
        //�������quit��ֱ���˳�������
        if (strcmp(buf, "quit") == 0) {
            printSysTime();
            cout << "[System Info]";
            cout << "�����˳�������" << endl;
            getchar();
            return 0;
        }
        if (len > 0) {
            printSysTime();
            cout << "���ͳɹ�" << endl;
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
            //ϵͳ��������Ϣ,�����Է���������ֱ�ӽ����߳�
            if (strcmp(buf, "exit") == 0) {
                return 0;
            }
            printf("[ϵͳ֪ͨ]%s\n",buf);
            continue;
        }
        //����user�����ֶ�
        for(int j=0;j<NAME_SIZE;j++)
            fromUser[j]=message[j+TYPE_SIZE];

        if (strcmp(buf, "quit") == 0) {
            printSysTime();
            printf("�û�[%s]�Ѿ��˳�������\n", fromUser);
            continue;
        }

        if(strcmp(type,"PRI")==0){
            printSysTime();
            printf("�û�[%s](˽��):%s\n",fromUser,buf);
        }
        else {
            printSysTime();
            printf("�û�[%s]:%s\n", fromUser, buf);
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
        //����ʧ��
        cout << "����DLLʧ��" << endl;
        return -1;
    }
    SOCKET sockClient = socket(AF_INET, SOCK_STREAM, 0);


    cout << "[NOT CONNECTED]����������������ĵ�ַ" << endl;
    cin >> ADDRSRV;

    SOCKADDR_IN addrSrv;
    addrSrv.sin_family = AF_INET;
    addrSrv.sin_port = htons(PORT);
    addrSrv.sin_addr.S_un.S_addr = inet_addr(ADDRSRV.c_str());

    if (connect(sockClient, (SOCKADDR *) &addrSrv, sizeof(SOCKADDR)) != 0) {
        //TCP����ʧ��
        cout << "����ʧ��" << endl;
        return -1;
    }
    cout << "[CONNECTED]SOCKET���ӳɹ�" << endl;
    //���û������ͻ��˱���
    while(1){
        cout << "[CONNECTED]�����������û���" << endl;
        cin >> userName;
        send(sockClient,userName,NAME_SIZE,0);

        char t[10];
        recv(sockClient,t,10,0);
        if(strcmp(t,"TRUE")==0){
            break;
        }
        cout<<"[CONNECTED]���û����Ѵ��ڣ�����������"<<endl;
    }
    cout<<"[CONNECTED]�����ӵ�������"<<endl;
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