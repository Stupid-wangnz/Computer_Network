# 实验3：基于UDP服务设计可靠传输协议并编程实现

**2011269 王楠舟**

**实验3-1：利用数据报套接字在用户空间实现面向连接的可靠数据传输，功能包括：建立连接、差错检测、确认重传等。流量控制采用停等机制，完成给定测试文件的传输。**

## 协议设计

### 数据报文结构

<img src="Lab3_1\img\image-20221113141915273.png" alt="image-20221113141915273" style="zoom:67%;" />

如图所示，报文头长度为`104Bits`，报文数据最大长度为`2048Bytes`。其中报文头前32位是`SEQ`，表示本次传输的报文的序列号；32-63位是`ACK`，表示对对应序列号报文传输成功的确认；64-79位为校验和，用于差错检测保证报文传输的无损性；80-95为数据长度，用于记录报文数据区域的有效数据大小；96-103位为标记位`Flag`，前四位为保留位全部置0，只有低四位有实际含义，分别为`END，FIN，ACK，SYN`。

![image-20221113143116709](Lab3_1\img\image-20221113143116709.png)

`END`标记位的作用：服务端在数据传输过程中是接收完全部数据后才将接收到的数据返回，写入文件，但是接收方不知道原始文件的长度。所以设计一个`END`标记位是用于客户端通知服务端文件已经传输完毕，可以写入文件了；

`FIN`标记位的作用：用于通知接收方断开连接；

`ACK`标记位的作用：用于通知发送方已经收到报文；

`SYN`标记位的作用：用于通过接收方连接连接。

### 差错检测--计算校验和

校验和的计算参考课程讲授内容。首先发送方产生伪首部，校验和字段初始化为0，将报文用0补齐为`16Bit`的整数倍，然后将报文看成`16Bit`整数序列，对序列中每一个元素进行二进制反码求和运算，计算结果取反写入报文头的校验和字段；

接收端收到数据报后用0补齐为`16Bit`的整数倍，按16位整数序列，采用 16 位二进制反码求和运算，如果计算结果为全1，则没有检测到错误，否则说明数据报传输时出错。

### 面向连接的数据传输：建立连接与断开连接

- **三次握手建立连接**

  参考TCP三次握手过程，为基于UDP服务的可靠传输协议设计类似的三次握手双向建连过程：

  <img src="Lab3_1\img\image-20221113151504892.png" alt="image-20221113151504892" style="zoom:67%;" />

  首先由客户端向服务端发出申请建立连接的报文，SYN标记位置为1，并初始化序列号`seq=x`告知服务端；服务端收到请求建立连接的报文后，向客户端回复一个同意建立连接并向客户端请求建立建立的报文，ACK和SYN标记位置为1，并设置`ack=x+1`表示确认收到`seq=x`的报文；客户端收到服务端的报文后进入`ESTABLISHED`状态，并回复给服务端一个ACK报文，`ack=y+1`表示确认收到`seq=y`的报文；服务端收到ACK报文后检查正确无误，进入`ESTABLISHED`状态。

  三次握手过程中客户端和服务端交换了序列号，双方都知道对方同意建立连接，并准备好了进行数据传输。

- **四次挥手断开连接**

  参考TCP四次挥手过程，为基于UDP服务的可靠传输协议设计类似的四次挥手双向断开连接过程：

  <img src="Lab3_1\img\image-20221113151527723.png" alt="image-20221113151527723" style="zoom:67%;" />

  首先由客户端向服务器发送断开连接的报文，FIN标记位置为1；服务端收到报文后回复一个ACK报文表示收到断开连接的请求；客户端收到ACK报文后等待服务端发送断开连接的报文；在这个阶段，客户端不能向服务端发送消息，但服务端能向客户端发送消息；随后，服务端向客户端发送断开连接的报文，FIN标记位置为1；客户端在收到服务端断开连接的报文后，回复给服务端ACK报文，然后进入`TIME_WAIT`状态，等待两个MSL后断开连接；服务端收到ACK报文后直接断开连接。

### 可靠数据传输：rdt3.0

为了实现可靠传输协议，我实现了rdt3.0用于保障数据传输过程的可靠性。由于只需要实现单向传输，所以客户端作为发送端，服务端作为接收端。

rdt3.0实现中，客户端发送`seq`号数据报后，需要等待对`seq`数据报的确认号`ack=seq`，并且检查数据报传输时无损，才能继续发送下一个数据报；如果超时未能收到正确无误的数据报，客户端就会重传一次`seq`数据报。服务器则是在等待`seq`数据报，如果等来的不是目标序列号数据报，服务端就会重新发送一次`ack`报文，直到服务端收到期望的序列号数据报。具体实现参考下图。

客户端有限状态机：

<img src="Lab3_1\img\image-20221113155717367.png" alt="image-20221113155717367" style="zoom: 50%;" />

服务器有限状态机：

<img src="Lab3_1\img\image-20221113155956146.png" alt="image-20221113155956146" style="zoom:50%;" />

## 具体代码实现

### 报文结构定义和一些宏定义

报文首部的定义：

```c++
#define SYN 0x1
#define ACK 0x2
#define FIN 0x4
#define END 0x8

struct packetHead {
    u_int seq;//序列号
    u_int ack;//确认号
    u_short checkSum;//校验和
    u_short bufSize;//数据段大小
    char flag;//标记位

    packetHead() {
        seq = ack = 0;
        checkSum = bufSize = 0;
        flag = 0;
    }
};
```

报文结构的定义：

```c++
#define MAX_DATA_SIZE 2048 //定义数据区域的最大长度
struct packet{
    packetHead head;
    char data[MAX_DATA_SIZE];
};
```

由于本次实验是最简单的停等机制，所以我们直接通过宏定义设置最大重传时间。

```c++
#define MAX_TIME = CLOCKS_PER_SEC;
```

### 计算校验和

将整个数据报看成一个16位的序列，`packetLen`是数据报的长度，单位是`Byte`。首先计算出以16位为长度单位该数据报的长度，然后在其末尾填充上0。接下来对整个序列进行16位的反码求和运算，将求和结果取反作为函数的返回值。

```c++
u_short checkPacketSum(u_short *packet, int packetLen) {

    u_long sum = 0;
    int count = (packetLen + 1) / 2;
	//填充为16位的整数倍
    u_short *temp = new u_short[count];
    memset(temp, 0, 2 * count);
    memcpy(temp, packet, packetLen);

    while (count--) {
        sum += *temp++;
        if (sum & 0xFFFF0000) {
            sum &= 0xFFFF;
            sum++;
        }
    }
    return ~(sum & 0xFFFF);
}
```

对于发送方而言，只需要产生一个伪数据报，调用该函数计算出校验和填充到校验和字段即可；对于接收方而言，需要对收到的数据报调用该函数，如果`checkPacketSum(packet,packetLen)==0`，证明数据报传输过程无误，反之。

### 阻塞模式与非阻塞模式

由于重传机制的实现需要对数据报的发送和接收进行计时，但是初始化socket时默认是阻塞态的socket，调用`recvfrom`函数后线程被阻塞，计时函数也不能正常运行。如果我们在阻塞态调用`recvfrom`那么计时函数就需要新开一个线程，为了避免这种麻烦，我们需要计时重传的阶段调用以下代码，将socket切换为非阻塞态。

```c++
u_long imode = 1;
ioctlsocket(sockClient, FIONBIO, &imode);//非阻塞

//or

u_long imode = 0;
ioctlsocket(sockClient, FIONBIO, &imode);//阻塞
```

### 客户端启动流程

```c++
	WSAData wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        //加载失败
        cout << "加载DLL失败" << endl;
        return -1;
    }
    SOCKET sockClient = socket(AF_INET, SOCK_DGRAM, 0);

    cout << "[NOT CONNECTED]请输入聊天服务器的地址" << endl;
    cin >> ADDRSRV;

    SOCKADDR_IN addrSrv;
    addrSrv.sin_family = AF_INET;
    addrSrv.sin_port = htons(PORT);
    addrSrv.sin_addr.S_un.S_addr = inet_addr(ADDRSRV.c_str());
    
	u_long imode = 1;
    ioctlsocket(sockClient, FIONBIO, &imode);//非阻塞
	//三次握手建立连接
    if (!connectToServer(sockClient, addrSrv)) {
        cout << "连接失败" << endl;
        return 0;
    }
	
	//读入需要传输的文件
    string filename;
    cout << "请输入需要传输的文件名" << endl;
    cin >> filename;
    ifstream infile(filename, ifstream::binary);
    if (!infile.is_open()) {
        cout << "无法打开文件" << endl;
        return 0;
    }
    infile.seekg(0, infile.end);
    u_long fileLen = infile.tellg();
    infile.seekg(0, infile.beg);
    //cout << fileLen << endl;
    char *fileBuffer = new char[fileLen];
    infile.read(fileBuffer, fileLen);
    infile.close();
    //cout.write(fileBuffer,fileLen);
    cout << "开始传输" << endl;
	
    sendFSM(fileLen, fileBuffer, sockClient, addrSrv);
	
	//四次挥手断开连接
    if (!disConnect(sockClient, addrSrv)) {
        cout << "断开失败" << endl;
        return 0;
    }

    return 1;
}
```

### 服务端启动过程

```c++
int main() {
    WSAData wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        //加载失败
        cout << "加载DLL失败" << endl;
        return -1;
    }
    SOCKET sockSrv = socket(AF_INET, SOCK_DGRAM, 0);

    SOCKADDR_IN addrSrv;
    addrSrv.sin_family = AF_INET;
    addrSrv.sin_port = htons(PORT);
    addrSrv.sin_addr.S_un.S_addr = inet_addr(ADDRSRV);
    bind(sockSrv, (SOCKADDR *) &addrSrv, sizeof(SOCKADDR));

    SOCKADDR_IN addrClient;
	
    //三次握手建立连接
    if (!acceptClient(sockSrv, addrClient)) {
        cout << "连接失败" << endl;
        return 0;
    }

    char fileBuffer[MAX_FILE_SIZE];
    //可靠数据传输过程
    u_long fileLen = recvFSM(fileBuffer, sockSrv, addrClient);
    //四次挥手断开连接
    if (!disConnect(sockSrv, addrClient)) {
        cout << "断开失败" << endl;
        return 0;
    }
    
	//写入复制文件
    string filename = R"(F:\Computer_network\Computer_Network\Lab3\Lab3_1\test_recv.txt)";
    ofstream outfile(filename, ios::binary);
    if (!outfile.is_open()) {
        cout << "打开文件出错" << endl;
        return 0;
    }
    cout << fileLen << endl;
    outfile.write(fileBuffer, fileLen);
    outfile.close();

    return 1;
}
```

### 三次握手

客户端三次握手过程：

```c++
bool connectToServer(SOCKET &socket, SOCKADDR_IN &addr) {
    int len = sizeof(addr);
    
    //第一次握手数据报
    packetHead head;
    head.flag |= SYN;
    head.seq = 0;
    head.checkSum = checkPacketSum((u_short *) &head, sizeof(head));

    char *buffer = new char[sizeof(head)];
    memcpy(buffer, &head, sizeof(head));
    sendto(socket, buffer, sizeof(head), 0, (sockaddr *) &addr, len);
    cout << "第一次握手成功" << endl;

    clock_t start = clock(); //开始计时
    while (recvfrom(socket, buffer, sizeof(head), 0, (sockaddr *) &addr, &len) <= 0) {
        if (clock() - start >= MAX_TIME) {
            memcpy(buffer, &head, sizeof(head));
            sendto(socket, buffer, sizeof(buffer), 0, (sockaddr *) &addr, len);
            start = clock();
        }
    }

    memcpy(&head, buffer, sizeof(head));
    if ((head.flag & ACK) && (checkPacketSum((u_short *) &head, sizeof(head)) == 0)) {
        cout << "第二次握手成功" << endl;
    } else {
        cout << "第二次握手失败" << endl;
        return false;
    }

    //服务器建立连接
    if (head.flag & SYN) {
        //第三次ACK报文准备
        head.flag = 0;
        head.flag |= ACK;
        head.checkSum = 0;
        head.checkSum = (checkPacketSum((u_short *) &head, sizeof(head)));
    } else {
        cout << "连接过程出错" << endl;
        return false;
    }
    memcpy(buffer, &head, sizeof(head));
    sendto(socket, buffer, sizeof(head), 0, (sockaddr *) &addr, len);

    //等待两个MAX_TIME，如果没有收到消息说明ACK没有丢包
    start = clock();
    while (clock() - start <= 2 * MAX_TIME) {
        if (recvfrom(socket, buffer, sizeof(packetHead), 0, (SOCKADDR *) &addr, &len) <= 0)
            continue;
        //说明这个ACK丢了
        memcpy(buffer, &head, sizeof(head));
        sendto(socket, buffer, sizeof(head), 0, (sockaddr *) &addr, len);
        start = clock();
    }
    cout << "三次握手完成，成功与服务器建立连接，可发送数据" << endl;
    return true;
}
```

为什么客户端发送完ACK报文不能立刻退出？这是因为由于只实现了简单的单向传输协议，但是必须考虑如果最后这个ACK报文发送丢包该怎么办：如果这个ACK报文发生丢包，即使服务端再怎么重传建立连接的报文，用户端也不能正确收到，因为用户端认为已经连接成功开始传输数据了，而服务端还在等待这个ACK报文。所以为了解决这个问题，用户端需要等待两个最大时间间隔，保证能收到服务器重传过来的数据报，如果两个最大时间间隔内都没收到服务端重传的请求连接报文则认为服务端收到ACK报文，客户端握手成功。

服务端三次握手过程：

```c++
bool acceptClient(SOCKET &socket, SOCKADDR_IN &addr) {

    char *buffer = new char[sizeof(packetHead)];
    int len = sizeof(addr);
    //阻塞态等待第一次握手
    recvfrom(socket, buffer, sizeof(packetHead), 0, (SOCKADDR *) &addr, &len);

    if ((((packetHead *) buffer)->flag & SYN) && (checkPacketSum((u_short *) buffer, sizeof(packetHead)) == 0))
        cout << "第一次握手成功" << endl;

    packetHead head;
    head.flag |= ACK;
    head.flag |= SYN;
    head.checkSum = checkPacketSum((u_short *) &head, sizeof(packetHead));
    memcpy(buffer, &head, sizeof(packetHead));
    if (sendto(socket, buffer, sizeof(packetHead), 0, (sockaddr *) &addr, len) == -1) {
        cout << "第二次握手失败" << endl;
        return false;
    }
    cout << "第二次握手成功" << endl;

    u_long imode = 1;
    ioctlsocket(socket, FIONBIO, &imode);//非阻塞
    clock_t start = clock(); //开始计时
    while (recvfrom(socket, buffer, sizeof(head), 0, (sockaddr *) &addr, &len) <= 0) {
        if (clock() - start >= MAX_TIME) {
            cout << "超时重传" << endl;
            sendto(socket, buffer, sizeof(buffer), 0, (sockaddr *) &addr, len);
            start = clock();
        }
    }

    if ((((packetHead *) buffer)->flag & ACK) && (checkPacketSum((u_short *) buffer, sizeof(packetHead)) == 0)) {
        cout << "第三次握手成功" << endl;
    } else {
        cout << "第三次握手失败" << endl;
        return false;
    }
    imode = 0;
    ioctlsocket(socket, FIONBIO, &imode);//阻塞
    return true;
}
```

### 可靠数据传输

#### 客户端实现

打包函数：负责将本次数据报的序列号和数据打包成一个完整的数据报，并计算号校验和放入该数据报中，最后数据报作为函数返回值。

```c++
packet makePacket(int seq, char *data, int len) {
    packet pkt;
    pkt.head.seq = seq;
    pkt.head.bufSize = len;
    memcpy(pkt.data, data, len);
    pkt.head.checkSum = checkPacketSum((u_short *) &pkt, sizeof(packet));
    return pkt;
}
```

发送端有限状态机的实现：`len`参数是需要传输数据的总的长度，`fileBuffer`参数是存放着原始数据的指针，`socket`是传输的套接字，`addr`是服务端地址。

实现原理是初始化当前`stage=0`，即准备发送`seq=0`的报文，发送成功后进入`stage=1`，等待`ack=0`；如果接收的ACK报文ack值不是0，则重发一次`seq=0`报文，维持在`stage=1`，如果ACK报文中`ack=0`，则进入`stage=2`，准备发送`seq=1`的报文，同理实现四个状态的有限状态机。最后数据传输完成后，发送一个`END`报文通知服务端文件已经传输完成，

```c++
void sendFSM(u_long len, char *fileBuffer, SOCKET &socket, SOCKADDR_IN &addr) {

    int packetNum = int(len / MAX_DATA_SIZE) + (len % MAX_DATA_SIZE ? 1 : 0);
    int index = 0;
    int packetDataLen = min(MAX_DATA_SIZE, len - index * MAX_DATA_SIZE);
    int stage = 0;

    int addrLen = sizeof(addr);
    clock_t start;

    char *data_buffer = new char[packetDataLen], *pkt_buffer = new char[sizeof(packet)];
    packet sendPkt, pkt;
    
    cout << "本次文件数据长度为" << len << "Bytes,需要传输" << packetNum << "个数据包" << endl;

    while (true) {

        if (index == packetNum) {
            packetHead endPacket;
            endPacket.flag |= END;
            endPacket.checkSum = checkPacketSum((u_short *) &endPacket, sizeof(packetHead));
            memcpy(pkt_buffer, &endPacket, sizeof(packetHead));
            sendto(socket, pkt_buffer, sizeof(packetHead), 0, (SOCKADDR *) &addr, addrLen);

            while (recvfrom(socket, pkt_buffer, sizeof(packetHead), 0, (SOCKADDR *) &addr, &addrLen) <= 0) {
                if (clock() - start >= MAX_TIME) {
                    memcpy(pkt_buffer, &endPacket, sizeof(packetHead));
                    sendto(socket, pkt_buffer, sizeof(packetHead), 0, (SOCKADDR *) &addr, addrLen);
                    start = clock();
                }
            }

            if(!(((packetHead*)(pkt_buffer))->flag & ACK) || checkPacketSum((u_short*)pkt_buffer, sizeof(packetHead))==0) {
                cout<<"文件传输完成"<<endl;
            }
            else
                continue;
            return;
        }
        packetDataLen = min(MAX_DATA_SIZE, len - index * MAX_DATA_SIZE);
        switch (stage) {
            case 0:
                memcpy(data_buffer, fileBuffer + index * MAX_DATA_SIZE, packetDataLen);
                sendPkt = makePacket(0, data_buffer, packetDataLen);

                memcpy(pkt_buffer, &sendPkt, sizeof(packet));
                sendto(socket, pkt_buffer, sizeof(packet), 0, (SOCKADDR *) &addr, addrLen);

                start = clock();//计时
                stage = 1;
                break;
            case 1:
                //time_out
                while (recvfrom(socket, pkt_buffer, sizeof(packet), 0, (SOCKADDR *) &addr, &addrLen) <= 0) {
                    if (clock() - start >= MAX_TIME) {
                        sendto(socket, pkt_buffer, sizeof(packet), 0, (SOCKADDR *) &addr, addrLen);
                        cout << index << "号数据包超时重传" << endl;
                        start = clock();
                    }
                }
                memcpy(&pkt, pkt_buffer, sizeof(packet));
                if (pkt.head.ack == 1 || checkPacketSum((u_short *) &pkt, sizeof(packet)) != 0) {
                    stage = 1;
                    break;
                }
                stage = 2;
                //cout<<index<<"号数据包传输成功，传输了"<<packetDataLen<<"Bytes数据"<<endl;
                index++;
                break;
            case 2:
                memcpy(data_buffer, fileBuffer + index * MAX_DATA_SIZE, packetDataLen);
                sendPkt = makePacket(1, data_buffer, packetDataLen);
                memcpy(pkt_buffer, &sendPkt, sizeof(packet));
                sendto(socket, pkt_buffer, sizeof(packet), 0, (SOCKADDR *) &addr, addrLen);

                start = clock();//计时
                stage = 3;
                break;
            case 3:
                //time_out
                while (recvfrom(socket, pkt_buffer, sizeof(packet), 0, (SOCKADDR *) &addr, &addrLen) <= 0) {
                    if (clock() - start >= MAX_TIME) {
                        sendto(socket, pkt_buffer, sizeof(packet), 0, (SOCKADDR *) &addr, addrLen);
                        cout << index << "号数据包超时重传" << endl;
                        start = clock();
                    }
                }
                memcpy(&pkt, pkt_buffer, sizeof(packet));
                if (pkt.head.ack == 0 || checkPacketSum((u_short *) &pkt, sizeof(packet)) != 0) {
                    stage = 3;
                    break;
                }
                stage = 0;
                //cout<<index<<"号数据包传输成功，传输了"<<packetDataLen<<"Bytes数据"<<endl;
                index++;
                break;
            default:
                cout << "error" << endl;
                return;
        }
    }
}
```

#### 服务端实现

打包函数：由于服务端只需要传输一个ACK报文，所以打包函数就是将报文首部的ack值设定为参数值，将`ACK`标记位置1，计算校验和，将打包好的数据报作为函数返回值。

```c++
packet makePacket(int ack) {
    packet pkt;
    pkt.head.ack = ack;
    pkt.head.flag |= ACK;
    pkt.head.checkSum = checkPacketSum((u_short *) &pkt, sizeof(packet));

    return pkt;
}
```

接收端有限状态机的实现：参数`fileBuffer`是将报文中的数据卸下来存入的目标缓存，`socket`是建连好的套接字，`addr`是客户端地址，函数返回值用于表示本次传输文件的总的长度。

实现原理如有限自动机图所示。服务端位于`stage=x`状态，服务端等待接收`seq=x`的报文，如果`recvfrom`接收到的报文首部`seq==x`，服务端接收该报文，将其数据段卸下存入`fileBuffer`，然后返回给客户端一个`ack=x`的ACK报文，接下来等待`seq=(x+1)%2`的报文；如果收到的报文`seq!=x`，说明是客户端重传的重复报文，服务端重新发送一个`ack=x`ACK报文给客户端。

由于服务端不知道何时结束文件接收，所以在每一个状态都需要判断首部`END`标记位是否有效，如果有效则返回ACK报文，将接收到的数据长度之和作为函数的返回值结束本次文件接收。

```c++
u_long recvFSM(char *fileBuffer, SOCKET &socket, SOCKADDR_IN &addr) {
    u_long fileLen = 0;
    int stage = 0;

    int addrLen = sizeof(addr);
    char *pkt_buffer = new char[sizeof(packet)];
    packet pkt, sendPkt;
    int index = 0;
    int dataLen;
    while (1) {
        memset(pkt_buffer, 0, sizeof(packet));
        switch (stage) {
            case 0:
                recvfrom(socket, pkt_buffer, sizeof(packet), 0, (SOCKADDR *) &addr, &addrLen);

                memcpy(&pkt, pkt_buffer, sizeof(packetHead));

                if (pkt.head.flag & END) {
                    cout << "传输完毕" << endl;
                    packetHead endPacket;
                    endPacket.flag |= ACK;
                    endPacket.checkSum = checkPacketSum((u_short *) &endPacket, sizeof(packetHead));
                    memcpy(pkt_buffer, &endPacket, sizeof(packetHead));
                    sendto(socket, pkt_buffer, sizeof(packetHead), 0, (SOCKADDR *) &addr, addrLen);
                    return fileLen;
                }

                memcpy(&pkt, pkt_buffer, sizeof(packet));

                if (pkt.head.seq == 1 || checkPacketSum((u_short *) &pkt, sizeof(packet)) != 0) {
                    sendPkt = makePacket(1);
                    memcpy(pkt_buffer, &sendPkt, sizeof(packet));
                    sendto(socket, pkt_buffer, sizeof(packet), 0, (SOCKADDR *) &addr, addrLen);
                    stage = 0;
                    cout << "收到重复的" << index - 1 << "号数据包，将其抛弃" << endl;
                    break;
                }

                //correctly receive the seq0
                dataLen = pkt.head.bufSize;
                memcpy(fileBuffer + fileLen, pkt.data, dataLen);
                fileLen += dataLen;

                //give back ack0
                sendPkt = makePacket(0);
                memcpy(pkt_buffer, &sendPkt, sizeof(packet));
                sendto(socket, pkt_buffer, sizeof(packet), 0, (SOCKADDR *) &addr, addrLen);
                stage = 1;

                //cout<<"成功收到"<<index<<"号数据包，其长度是"<<dataLen<<endl;
                index++;
                break;
            case 1:
                recvfrom(socket, pkt_buffer, sizeof(packet), 0, (SOCKADDR *) &addr, &addrLen);

                memcpy(&pkt, pkt_buffer, sizeof(packetHead));

                if (pkt.head.flag & END) {
                    cout << "传输完毕" << endl;
                    packetHead endPacket;
                    endPacket.flag |= ACK;
                    endPacket.checkSum = checkPacketSum((u_short *) &endPacket, sizeof(packetHead));
                    memcpy(pkt_buffer, &endPacket, sizeof(packetHead));
                    sendto(socket, pkt_buffer, sizeof(packetHead), 0, (SOCKADDR *) &addr, addrLen);
                    return fileLen;
                }

                memcpy(&pkt, pkt_buffer, sizeof(packet));

                if (pkt.head.seq == 0 || checkPacketSum((u_short *) &pkt, sizeof(packet)) != 0) {
                    sendPkt = makePacket(0);
                    memcpy(pkt_buffer, &sendPkt, sizeof(packet));
                    sendto(socket, pkt_buffer, sizeof(packet), 0, (SOCKADDR *) &addr, addrLen);
                    stage = 1;
                    cout << "收到重复的" << index - 1 << "号数据包，将其抛弃" << endl;
                    break;
                }

                //correctly receive the seq1
                dataLen = pkt.head.bufSize;
                memcpy(fileBuffer + fileLen, pkt.data, dataLen);
                fileLen += dataLen;

                //give back ack1
                sendPkt = makePacket(1);
                memcpy(pkt_buffer, &sendPkt, sizeof(packet));
                sendto(socket, pkt_buffer, sizeof(packet), 0, (SOCKADDR *) &addr, addrLen);
                stage = 0;

                //cout<<"成功收到"<<index<<"号数据包，其长度是"<<dataLen<<endl;
                index++;

                break;
        }
    }
}
```

### 四次挥手

由客户端主动发起断连请求，服务端对用户端的断连做应答，在同意客户端断连后服务端立刻发送一个断连请求给客户端，由客户端对此应答。

客户端的四次挥手过程：

```c++
bool disConnect(SOCKET &socket, SOCKADDR_IN &addr) {

    int addrLen = sizeof(addr);
    char *buffer = new char[sizeof(packetHead)];
    packetHead closeHead;
    closeHead.flag |= FIN;
    closeHead.checkSum = checkPacketSum((u_short *) &closeHead, sizeof(packetHead));

    memcpy(buffer, &closeHead, sizeof(packetHead));
    sendto(socket, buffer, sizeof(packetHead), 0, (SOCKADDR *) &addr, addrLen);

    clock_t start = clock();
    while (recvfrom(socket, buffer, sizeof(packetHead), 0, (sockaddr *) &addr, &addrLen) <= 0) {
        if (clock() - start >= MAX_TIME) {
            memcpy(buffer, &closeHead, sizeof(packetHead));
            sendto(socket, buffer, sizeof(packetHead), 0, (SOCKADDR *) &addr, addrLen);
            start = clock();
        }
    }

    if ((((packetHead *) buffer)->flag & ACK) && (checkPacketSum((u_short *) buffer, sizeof(packetHead) == 0))) {
        cout << "客户端已经断开" << endl;
    } else {
        cout << "错误发生，程序中断" << endl;
        return false;
    }

    u_long imode = 0;
    ioctlsocket(socket, FIONBIO, &imode);//阻塞

    recvfrom(socket, buffer, sizeof(packetHead), 0, (SOCKADDR *) &addr, &addrLen);

    if ((((packetHead *) buffer)->flag & FIN) && (checkPacketSum((u_short *) buffer, sizeof(packetHead) == 0))) {
        cout << "服务器断开" << endl;
    } else {
        cout << "错误发生，程序中断" << endl;
        return false;
    }

    imode = 1;
    ioctlsocket(socket, FIONBIO, &imode);

    closeHead.flag = 0;
    closeHead.flag |= ACK;
    closeHead.checkSum = checkPacketSum((u_short *) &closeHead, sizeof(packetHead));

    memcpy(buffer, &closeHead, sizeof(packetHead));
    sendto(socket, buffer, sizeof(packetHead), 0, (SOCKADDR *) &addr, addrLen);
    start = clock();
    while (clock() - start <= 2 * MAX_TIME) {
        if (recvfrom(socket, buffer, sizeof(packetHead), 0, (SOCKADDR *) &addr, &addrLen) <= 0)
            continue;
        //说明这个ACK丢了
        memcpy(buffer, &closeHead, sizeof(packetHead));
        sendto(socket, buffer, sizeof(packetHead), 0, (sockaddr *) &addr, addrLen);
        start = clock();
    }

    cout << "连接关闭" << endl;
    closesocket(socket);
    return true;
}
```

服务端的四次挥手过程：

```c++
bool disConnect(SOCKET &socket, SOCKADDR_IN &addr) {
    int addrLen = sizeof(addr);
    char *buffer = new char[sizeof(packetHead)];

    recvfrom(socket, buffer, sizeof(packetHead), 0, (SOCKADDR *) &addr, &addrLen);
    if ((((packetHead *) buffer)->flag & FIN) && (checkPacketSum((u_short *) buffer, sizeof(packetHead) == 0))) {
        cout << "用户端断开" << endl;
    } else {
        cout << "错误发生，程序中断" << endl;
        return false;
    }

    packetHead closeHead;
    closeHead.flag = 0;
    closeHead.flag |= ACK;
    closeHead.checkSum = checkPacketSum((u_short *) &closeHead, sizeof(packetHead));
    memcpy(buffer, &closeHead, sizeof(packetHead));
    sendto(socket, buffer, sizeof(packetHead), 0, (SOCKADDR *) &addr, addrLen);

    closeHead.flag |= FIN;
    closeHead.checkSum = checkPacketSum((u_short *) &closeHead, sizeof(packetHead));
    memcpy(buffer, &closeHead, sizeof(packetHead));
    sendto(socket, buffer, sizeof(packetHead), 0, (SOCKADDR *) &addr, addrLen);

    u_long imode = 1;
    ioctlsocket(socket, FIONBIO, &imode);
    clock_t start = clock();
    while (recvfrom(socket, buffer, sizeof(packetHead), 0, (sockaddr *) &addr, &addrLen) <= 0) {
        if (clock() - start >= MAX_TIME) {
            memcpy(buffer, &closeHead, sizeof(packetHead));
            sendto(socket, buffer, sizeof(packetHead), 0, (SOCKADDR *) &addr, addrLen);
            start = clock();
        }
    }

    if ((((packetHead *) buffer)->flag & ACK) && (checkPacketSum((u_short *) buffer, sizeof(packetHead) == 0))) {
        cout << "链接关闭" << endl;
    } else {
        cout << "错误发生，程序中断" << endl;
        return false;
    }
    closesocket(socket);
    return true;
}
```

## 实验结果展示

### 对丢包的测试

启动`Router`程序，设置如下:

<img src="Lab3_1\img\image-20221113191054513.png" alt="image-20221113191054513" style="zoom: 67%;" />

```
//client端设置
#define PORT 7879

//server端设置
#define PORT 7878
#define ADDRSRV "127.0.0.1"
```

**三次握手测试：**

<img src="Lab3_1\img\image-20221113191236919.png" alt="image-20221113191236919" style="zoom:67%;" />

**文件传输过程和四次挥手断开连接：**

<img src="Lab3_1\img\image-20221113192328432.png" alt="image-20221113192328432" style="zoom:67%;" />

可以见到丢包率为5%，在客户端能看到5%的重传次数，并且接收到文件长度为`685421Byte`和在客户端读入文件时确认的长度一致，证明传输的可靠性。

可以通过在ShowLog打印出更详细的日志：

<img src="Lab3_1\img\image-20221119133200622.png" alt="image-20221119133200622" style="zoom:67%;" />

<img src="Lab3_1\img\image-20221119133235899.png" alt="image-20221119133235899" style="zoom:67%;" />

**传输结果对比：**

<img src="Lab3_1\img\image-20221113192640466.png" alt="image-20221113192640466" style="zoom:67%;" />

![image-20221119133314159](Lab3_1\img\image-20221119133314159.png)

![image-20221119133336063](Lab3_1\img\image-20221119133336063.png)

![image-20221119133348934](Lab3_1\img\image-20221119133348934.png)

可见无论哪种类型的文件，传输前后都是一致的，验证了传输的可靠性。

## GitHub仓库

[Computer_Network/Lab3/Lab3_1 at main · Stupid-wangnz/Computer_Network (github.com)](https://github.com/Stupid-wangnz/Computer_Network/tree/main/Lab3/Lab3_1)