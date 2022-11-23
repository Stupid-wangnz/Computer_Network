# 实验3：基于UDP服务设计可靠传输协议并编程实现

**2011269 王楠舟**

**实验3-2：**在实验3-1的基础上，将停等机制改成基于**滑动窗口的流量控制机制**，采用固定窗口大小，支持**累积确认**，完成给定测试文件的传输。

## 协议设计

### 数据报文结构

<img src="Lab3_1\img\image-20221113141915273.png" alt="image-20221113141915273" style="zoom:67%;" />

如图所示，报文头长度为`104Bits`，报文数据最大长度为`2048Bytes`。其中报文头前32位是`SEQ`，表示本次传输的报文的序列号；32-63位是`ACK`，表示对对应序列号报文传输成功的确认；为了支持**多个序列号实现滑动窗口的流量控制机制**，在数据报文设计上，`SEQ`和`ACK`长度都为`32Bit`。

64-79位为校验和，用于差错检测保证报文传输的无损性；80-95为数据长度，用于记录报文数据区域的有效数据大小；96-103位为标记位`Flag`，前四位为保留位全部置0，只有低四位有实际含义，分别为`END，FIN，ACK，SYN`。

![image-20221113143116709](F:\Computer_network\Computer_Network\Lab3\Lab3_1\img\image-20221113143116709.png)

`END`标记位的作用：服务端在数据传输过程中是接收完全部数据后才将接收到的数据返回，写入文件，但是接收方不知道原始文件的长度。所以设计一个`END`标记位是用于客户端通知服务端文件已经传输完毕，可以写入文件了；

`FIN`标记位的作用：用于通知接收方断开连接；

`ACK`标记位的作用：用于通知发送方已经收到报文；

`SYN`标记位的作用：用于通过接收方连接连接。

### 面向连接的数据传输：建立连接与断开连接

- **三次握手建立连接**

参考TCP三次握手过程，为基于UDP服务的可靠传输协议设计类似的三次握手双向建连过程：

<img src="Lab3_1\img\image-20221113151504892.png" alt="image-20221113151504892" style="zoom:67%;" />

首先由客户端向服务端发出申请建立连接的报文，SYN标记位置为1，并初始化序列号`seq=x`告知服务端；服务端收到请求建立连接的报文后，向客户端回复一个同意建立连接并向客户端请求建立建立的报文，ACK和SYN标记位置为1，并设置`ack=x+1`表示确认收到`seq=x`的报文；客户端收到服务端的报文后进入`ESTABLISHED`状态，并回复给服务端一个ACK报文，`ack=y+1`表示确认收到`seq=y`的报文；服务端收到ACK报文后检查正确无误，进入`ESTABLISHED`状态。

**在三次握手阶段，第一次客户端发出`SYN`包，数据报头的`SEQ`字段携带了客户端的初始化序列号，将其交给服务端，服务器收到后将`expectedSeq`初始化为`SEQ`中的值，从而在握手建立连接阶段将双方序列号同步。**

- **四次挥手断开连接**

参考TCP四次挥手过程，为基于UDP服务的可靠传输协议设计类似的四次挥手双向断开连接过程：

<img src="F:\Computer_network\Computer_Network\Lab3\Lab3_1\img\image-20221113151527723.png" alt="image-20221113151527723" style="zoom:67%;" />

首先由客户端向服务器发送断开连接的报文，FIN标记位置为1；服务端收到报文后回复一个ACK报文表示收到断开连接的请求；客户端收到ACK报文后等待服务端发送断开连接的报文；在这个阶段，客户端不能向服务端发送消息，但服务端能向客户端发送消息；随后，服务端向客户端发送断开连接的报文，FIN标记位置为1；客户端在收到服务端断开连接的报文后，回复给服务端ACK报文，然后进入`TIME_WAIT`状态，等待两个MSL后断开连接；服务端收到ACK报文后直接断开连接。

### 传输协议：使用基于滑动窗口的流量控制机制的rdt3.0

重传策略：`Go Back N`，发送端有限自动机如下：

<img src="Lab3_2\img\image-20221121211902858.png" alt="image-20221121211902858" style="zoom: 80%;" />

接收端有限自动机如下：

<img src="Lab3_2\img\image-20221121212113398.png" alt="image-20221121212113398" style="zoom:80%;" />

为了实现基于滑动窗口的流量控制机制，发送端和接收端都维护了各自的缓冲区，如图所示为发送端的发射缓冲区：

<img src="C:\Users\LEGION\AppData\Roaming\Typora\typora-user-images\image-20221122172600157.png" alt="image-20221122172600157" style="zoom:80%;" />

由于只需要实现简单的固定窗口大小，具体流量控制机制实现的方法是：

1. 接收端在建立连接时，将接收端窗口大小告知发送端，发送端收到后初始化窗口大小；
2. 在接收端，接收窗口大小始终不变，每有一个按序列号顺序达到的报文就将数据内容摘下，保存到接收端；
3. 发送端在传输阶段，不需要每次发送一条报文就等待ACK应答，一次性最多能发送发送端窗口大小的全部数据，`nextseqnum`指出发送端下一个数据报的序列号。若超出窗口大小，则该报文不发送；若在发射窗口内，则将数据保存到缓冲区，然后发送给接收端。如果发送端超时未能收到一条`ACK`报文，就会重传`sendbase~nextseqnum`之间的全部报文。

### 累积确认

为了实现累积确认，发送端和接收端的实现与停等协议中的不同如下：

1. 接收端缓存按序收到的最大的序列号+1，即`expectededSeqNum`，如果接收端收到的报文序列号不是`expectededSeqNum`，则重传一个对`expectededSeqNum`-1的`ACK`报文给发送端；
2. 发送端如果接收到接收端传来的`ACK`报文，首先判断`ACK`报文是否确认的是发射窗口内的消息，如果不是，则抛弃这个报文；如果是，则将`sendbase`赋值为`ack+1`，即将窗口滑动到该`ACK`报文确认的序列号的后一个位置。由于是累积确认，窗口的滑动可以一次滑动多个序列号位置。

## 具体代码实现

```c++
//定义最大序列号，若超过则取模
#define MAX_SEQ 0xffff
```

### `Client`端实现滑动窗口：

```c++
static int windowSize = 16;
static unsigned int base = 0;//握手阶段确定的初始序列号

//判断seq是否在窗口内
bool inWindows(u_int seq) {
    if (seq >= base && seq < base + windowSize)
        return true;
    if (seq < base && seq < ((base + windowSize) % MAX_SEQ))
        return true;

    return false;
}

//用于判断nextSeq序列号在窗口中的偏移量
u_int waitingNum(u_int nextSeq) {
    if (nextSeq >= base)
        return nextSeq - base;
    return nextSeq + MAX_SEQ - base;
}
```

为了实现滑动窗口，在发送端中以`Packet sendPkt[windowSize];`形式定义该滑动窗口，每当发送新的数据包就保存到数组中，当收到`ACK`报文，窗口向前滑动即将在数组靠后的值赋给前面的值；当超时重传，将数组中全部数据包重传一次。

`base、nextSeqNum、windowSize`维护了当前的滑动窗口状态，`[base,nextSeqNum]`之间是已发送但未被确认的数据包，`[nextSeqNum,base+windowSize]`是滑动窗口中未使用的缓存空间。具体实现代码逻辑如下：

```c++
void sendFSM(u_long len, char *fileBuffer, SOCKET &socket, SOCKADDR_IN &addr) {

    int packetNum = int(len / MAX_DATA_SIZE) + (len % MAX_DATA_SIZE ? 1 : 0);
    //sendIndex==packetNum时，不再发送；recvIndex==packetNum，收到全部ACK，结束传输
    int sendIndex = 0, recvIndex = 0;
    int packetDataLen=0;
    int addrLen = sizeof(addr);
    
    clock_t start;
    bool stopTimer = false;//是否停止计时
    char *data_buffer = new char[packetDataLen], *pkt_buffer = new char[sizeof(Packet)];
    Packet recvPkt;
    u_int nextSeqNum = base;
    
    Packet sendPkt[windowSize];
    cout << "本次文件数据长度为" << len << "Bytes,需要传输" << packetNum << "个数据包" << endl;

    while (true) {
        if (recvIndex == packetNum) {
            //recv全部ACK，结束传输，发送END报文
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
                cout << "文件传输完成" << endl;
                return;
            }

            resend:
            continue;
        }

        packetDataLen = min(MAX_DATA_SIZE, len - sendIndex * MAX_DATA_SIZE);

        //如果下一个序列号在滑动窗口中
        if (inWindows(nextSeqNum) && sendIndex < packetNum) {
			
            memcpy(data_buffer, fileBuffer + sendIndex * MAX_DATA_SIZE, packetDataLen);
            //缓存进入数组
            sendPkt[(int) waitingNum(nextSeqNum)] = makePacket(nextSeqNum, data_buffer, packetDataLen);
            memcpy(pkt_buffer, &sendPkt[(int) waitingNum(nextSeqNum)], sizeof(Packet));
           	//发送给接收端
            sendto(socket, pkt_buffer, sizeof(Packet), 0, (SOCKADDR *) &addr, addrLen);
			
            //如果目前窗口中只有一个数据报，开始计时（整个窗口共用一个计时器）
            if (base == nextSeqNum) {
                start = clock();
                stopTimer = false;
            }
            nextSeqNum = (nextSeqNum + 1) % MAX_SEQ;
            sendIndex++;
            cout << sendIndex << "号数据包已经发送" << endl;
        }
		//判断是否有ACK到来
        while (recvfrom(socket, pkt_buffer, sizeof(Packet), 0, (SOCKADDR *) &addr, &addrLen) > 0) {
            memcpy(&recvPkt, pkt_buffer, sizeof(Packet));
            //corrupt
            if (CheckPacketSum((u_short *) &recvPkt, sizeof(Packet)) != 0 || !(recvPkt.head.flag & ACK))
                goto time_out;
            //not corrupt
            if (base < (recvPkt.head.ack + 1)) {
                //不是窗口外的ACK
                int d = recvPkt.head.ack + 1 - base;
                for (int i = 0; i < (int) waitingNum(nextSeqNum) - d; i++) {
                    sendPkt[i] = sendPkt[i + d];
                }
                recvIndex+=d;
                cout << "[base:" << base << "\tnextSeq:" << nextSeqNum << "\tendWindow:" << base + windowSize << endl;
            }
            base = (max((recvPkt.head.ack + 1), base)) % MAX_SEQ;
            //当窗口为空，停止计时
            if (base == nextSeqNum)
                stopTimer = true;
            else {
                start = clock();
                stopTimer = false;
            }

        }
		//超时发生，将数组中缓存的数据报全部重传一次，这就是Go Back N
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
```

### `Server`端实现滑动窗口

```
//在握手阶段初始化base_stage,Client的初始化序列号
static u_int base_stage = 0;
```

`Server`端由于保证了窗口大小不变，对按顺序到达的数据包直接取下数据段复制到输出文件中；同时`Server`中缓存了一个答应按序达到最大序列号的`ACK`报文，如果收到一个错序的数据报，`Server`端直接抛弃并将这个缓存的`ACK`报文传输给`Client`.具体代码实现如下：

```c++
u_long recvFSM(char *fileBuffer, SOCKET &socket, SOCKADDR_IN &addr) {
    u_long fileLen = 0;
    int addrLen = sizeof(addr);
    u_int expectedSeq = base_stage;
    int dataLen;

    char *pkt_buffer = new char[sizeof(Packet)];
    Packet recvPkt, sendPkt= makePacket(base_stage - 1);//缓存的ACK报文

    while (true) {
        memset(pkt_buffer, 0, sizeof(Packet));
        recvfrom(socket, pkt_buffer, sizeof(Packet), 0, (SOCKADDR *) &addr, &addrLen);
        memcpy(&recvPkt,pkt_buffer, sizeof(Packet));

        if (recvPkt.head.flag & END && CheckPacketSum((u_short *) &recvPkt, sizeof(PacketHead)) == 0) {
            cout << "传输完毕" << endl;
            PacketHead endPacket;
            endPacket.flag |= ACK;
            endPacket.checkSum = CheckPacketSum((u_short *) &endPacket, sizeof(PacketHead));
            memcpy(pkt_buffer, &endPacket, sizeof(PacketHead));
            sendto(socket, pkt_buffer, sizeof(PacketHead), 0, (SOCKADDR *) &addr, addrLen);
            return fileLen;
        }

        if(recvPkt.head.seq==expectedSeq && CheckPacketSum((u_short *) &recvPkt, sizeof(Packet)) == 0){
            //correctly receive the expected seq
            dataLen = recvPkt.head.bufSize;
            memcpy(fileBuffer + fileLen, recvPkt.data, dataLen);
            fileLen += dataLen;

            //give back ack=seq
            sendPkt = makePacket(expectedSeq);
            memcpy(pkt_buffer, &sendPkt, sizeof(Packet));
            sendto(socket, pkt_buffer, sizeof(Packet), 0, (SOCKADDR *) &addr, addrLen);
            expectedSeq=(expectedSeq+1)%MAX_SEQ;
            continue;
        }
        //不是期望的Seq或传输过程出错，重传ACK
        cout<<"wait head:"<<expectedSeq<<endl;
        cout<<"recv head:"<<recvPkt.head.seq<<endl;
        memcpy(pkt_buffer, &sendPkt, sizeof(Packet));
        sendto(socket, pkt_buffer, sizeof(Packet), 0, (SOCKADDR *) &addr, addrLen);
    }
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