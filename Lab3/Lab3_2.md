# 实验3：基于UDP服务设计可靠传输协议并编程实现

**2011269 王楠舟**

**实验3-2：**在实验3-1的基础上，将停等机制改成基于**滑动窗口的流量控制机制**，采用固定窗口大小，支持**累积确认**，完成给定测试文件的传输。

## 程序功能介绍

1. 在实验3-1的基础上，实现流水线协议，并基于滑动窗口实现流量控制机制；
2. 重传机制使用`Go Back N`；
3. 在发送端、接收端设计缓冲区缓存数据，缓冲区大小取决于滑动窗口大小；
4. 实现累积确认，接收端等待一定时间的按序到达报文后再发送`ACK`报文，避免窗口的频繁滑动而需要频繁更新缓冲区，造成性能的损失；
5. 发送端使用双线程，分别负责报文的发送和接收。

## 协议设计

### 数据报文结构

<img src="Lab3_2\img\image-20221125160247325.png" alt="image-20221125160247325" style="zoom:67%;" />

如图所示，报文头长度为`112Bits`。对比在`Lab3_1`中设计的报文，`Lab3_2`中在报文头**新增`WindowSize`字段**，用于指出**接收端的接收缓冲区窗口大小**，由于在本次实验中固定窗口大小，将该字段设为固定值即可。

### 差错检测--计算校验和

首先发送方产生伪首部，校验和字段初始化为0，将报文用0补齐为`16Bit`的整数倍，然后将报文看成`16Bit`整数序列，对序列中每一个元素进行二进制反码求和运算，计算结果取反写入报文头的校验和字段；接收端收到数据报后用0补齐为`16Bit`的整数倍，按16位整数序列，采用 16 位二进制反码求和运算，如果计算结果为全1，则没有检测到错误，否则说明数据报传输时出错。

### 面向连接的数据传输：建立连接与断开连接

实现与TCP类似的三次握手、四次挥手过程，由于是Lab3_1的内容，不再赘述。

**新增：建立连接过程中对序列号的同步**

<img src="Lab3_1\img\image-20221113151504892.png" alt="image-20221113151504892" style="zoom:67%;" />

**在三次握手阶段，第一次客户端发出`SYN`包，数据报头的`SEQ`字段携带了客户端的初始化序列号，将其交给服务端，服务器收到后将`expectedSeq`初始化为`SEQ`中的值，从而在握手建立连接阶段将双方序列号同步。**

### 传输协议：使用基于滑动窗口的流量控制机制的rdt3.0

重传策略：`Go Back N`，发送端有限自动机如下：

<img src="Lab3_2\img\image-20221121211902858.png" alt="image-20221121211902858" style="zoom: 80%;" />

接收端有限自动机如下：

<img src="Lab3_2\img\image-20221121212113398.png" alt="image-20221121212113398" style="zoom:80%;" />

为了实现基于滑动窗口的流量控制机制，发送端和接收端都维护了各自的缓冲区，如图所示为发送端的发射缓冲区：

<img src="C:\Users\LEGION\AppData\Roaming\Typora\typora-user-images\image-20221122172600157.png" alt="image-20221122172600157" style="zoom:80%;" />

由于只需要实现简单的固定窗口大小，具体流量控制机制实现的方法是：

1. 接收端在建立连接时，将接收端窗口大小告知发送端，发送端收到后初始化窗口大小；
2. 在**接收端**，接收窗口大小始终不变，每有一个按序列号顺序达到的报文就将数据内容摘下并保存。
3. **发送端的发送线程**在传输阶段，不需要每次发送一条报文就等待ACK应答，一次性最多能发送发送端窗口大小的全部数据，`nextseqnum`指出发送端下一个数据报的序列号。若超出窗口大小，则该报文不发送；**若在发射窗口内，则将数据保存到缓冲区，然后发送给接收端**。如果发送端超时未能收到一条`ACK`报文，就会重传`sendbase~nextseqnum`之间的全部报文。
4. **发送端处理ACK报文的线程**，每当接收到一条来自接收到应答的`ACK`报文，将报文头部的`ack序列号+1`作为`sendbase`的更新值，然后将缓冲区中的更新，替换掉`sendbase~ack+1`之间的全部缓冲报文，从而实现窗口的滑动。

### 累积确认

为了实现累积确认，发送端和接收端的实现与停等协议中的不同如下：

1. 缓存按序收到的最大的序列号+1，即`expectededSeqNum`。如果如果接收端收到的报文序列号是`expectededSeqNum`，接收端会缓存对`expectedSeqNum`的`ACK`报文。但是接收端并不会马上返回该`ACK`报文，而是**等待`MAX_WAIT_TIME`的时长**，如果接下来的报文是按序到达的则继续等待并更新缓存的`ACK`报文，直到超时或者接收到错序的报文，就将缓存的`ACK`报文作为应答回复给发送端。
2. 发送端如果接收到接收端传来的`ACK`报文，首先判断`ACK`报文是否确认的是发射窗口内的消息，如果不是，则抛弃这个报文；如果是，则将`sendbase`赋值为`ack+1`，即将窗口滑动到该`ACK`报文确认的序列号的后一个位置。由于是累积确认，窗口的滑动可以一次滑动多个序列号位置。

## 具体代码实现

```c++
//定义最大序列号，若超过则取模
#define MAX_SEQ 0xffff

//多线程中共享变量
static int windowSize = 16;
static unsigned int base = 0;//握手阶段确定的初始序列号
static unsigned int nextSeqNum = 0;
static Packet *sendPkt = nullptr;
static bool stopTimer = false;
static clock_t start;
static int packetNum;
```

### `Client`端实现滑动窗口：

```c++
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

为了实现滑动窗口，在发送端中以`Packet sendPkt[windowSize];`形式定义该滑动窗口，每当发送新的数据包就保存到数组中，当收到`ACK`报文，窗口向前滑动即将在数组靠后的值赋给前面的值；当超时重传，将数组中全部数据包重传一次。`base、nextSeqNum、windowSize`维护了当前的滑动窗口状态，`[base,nextSeqNum]`之间是已发送但未被确认的数据包，`[nextSeqNum,base+windowSize]`是滑动窗口中未使用的缓存空间。

为了优化代码，我的程序设计中**主线程**只负责发送窗口内的数据，为了处理应答的`ACK`报文，**新开一个线程**处理应答报文并负责窗口的滑动。具体实现代码逻辑如下：

**处理`ACK`报文的线程实现方法:**该线程始终在调用`recvfrom`函数等待报文到达（由于是非阻塞模式下，所以必须通过

`if (recvfrom(*clientSock, recvBuffer, sizeof(Packet), 0, (SOCKADDR *) &addrSrv, &addrLen) > 0) `判断是否有报文到达）。接下来该线程判断`ack`值范围是否在窗口内，如果是则负责将`base`更新为`recvPacket.head.ack + 1`，并且更新缓冲区`sendPkt`数组，但是在更新中必须对共享变量加锁，否则会出现多线程之间的数据冲突。

如果窗口在滑动后没有正在等待被确认的报文了，就关闭计时器；否则，计时器更新重新启动。如果` (base == packetNum)`为真，意味着数据已经全部传输完毕了，该线程函数以及退出，否则在后续断开连接阶段会影响程序正常运行。

```c++
//处理ACK报文的线程
DWORD WINAPI ACKHandler(LPVOID param) {
    SOCKET *clientSock = (SOCKET *) param;
 
    while (true) {
        if (recvfrom(*clientSock, recvBuffer, sizeof(Packet), 0, (SOCKADDR *) &addrSrv, &addrLen) > 0) {
            memcpy(&recvPacket, recvBuffer, sizeof(Packet));
            
           if (CheckPacketSum((u_short *) &recvPacket, sizeof(Packet)) == 0 && recvPacket.head.flag & ACK) {
               if (base < (recvPacket.head.ack + 1)) {
                    int d = recvPacket.head.ack + 1 - base;
                    //互斥锁，避免线程对共享变量的操作出现数据冲突 
                   	mutexLock.lock();
                    assert(sendPkt != nullptr);
                    for (int i = 0; i < (int) waitingNum(nextSeqNum) - d; i++) {
                        sendPkt[i] = sendPkt[i + d];
                    }
                    base = (max((recvPacket.head.ack + 1), base)) % MAX_SEQ;
                    mutexLock.unlock();
                   
                    cout << "[window move]base:" << base << " nextSeq:" << nextSeqNum << " endWindow:"
                         << base + windowSize << endl;
                    
                    if (base == packetNum)
                        return 0;
                }
                if (base == nextSeqNum)
                    stopTimer = true;
                else {
                    start = clock();
                    stopTimer = false;
                }
            }
        }
    }
}
```

**发送线程：**首先判断准备发送数据的序列号`nextSeqNum`是否在滑动窗口中，如果是则将数据打包成数据报文，并通过

`sendPkt[(int) waitingNum(nextSeqNum)] = makePacket(nextSeqNum, data_buffer, packetDataLen);`保存到缓冲区中。最后调用`sendto`函数发送给接收端，由于整个窗口共用一个计时器，如果判断该报文是窗口中第一个发出的数据，则启动计时器。同理，为了保证共享数据的一致性，在更新缓冲区时必须使用互斥锁来避免产生数据冲突。

```c++
packetDataLen = min(MAX_DATA_SIZE, len - sendIndex * MAX_DATA_SIZE);

if (inWindows(nextSeqNum) && sendIndex < packetNum) {
    memcpy(data_buffer, fileBuffer + sendIndex * MAX_DATA_SIZE, packetDataLen);

    assert (sendPkt != nullptr);
    mutexLock.lock();
    sendPkt[(int) waitingNum(nextSeqNum)] = makePacket(nextSeqNum, data_buffer, packetDataLen);
    memcpy(pkt_buffer, &sendPkt[(int) waitingNum(nextSeqNum)], sizeof(Packet));
    ShowPacket(&sendPkt[(int) waitingNum(nextSeqNum)]);
    mutexLock.unlock();
    sendto(socket, pkt_buffer, sizeof(Packet), 0, (SOCKADDR *) &addr, addrLen);

    if (base == nextSeqNum) {
    	start = clock();
        stopTimer = false;
    }
    nextSeqNum = (nextSeqNum + 1) % MAX_SEQ;
    sendIndex++;
}
```

如果该窗口超时，则需要将缓冲区里的数据全部重传一次，即将缓冲数组遍历发送给接收端，并重新启动计时。

```c++
time_out:
if (!stopTimer && clock() - start >= MAX_TIME) {
    cout << "[time out!]resend begin" << endl;
    mutexLock.lock();
    for (int i = 0; i < (int) waitingNum(nextSeqNum); i++) {
        memcpy(pkt_buffer, &sendPkt[i], sizeof(Packet));
        sendto(socket, pkt_buffer, sizeof(Packet), 0, (SOCKADDR *) &addr, addrLen);
        ShowPacket(&sendPkt[i]);
    }
    mutexLock.unlock();
    start = clock();
    stopTimer = false;
}
```

最后发送线程的实现不停循环发送数据和判断超时，知道全部数据发送完毕再退出。

### `Server`端实现滑动窗口

接收端中缓存了`sendPkt`作为对最后一次按序到达的报文的`ACK`应答报文。

```
//在握手阶段初始化base_stage,Client的初始化序列号
static u_int base_stage = 0;
static int windowSize = 16;

//初始化接收端时
u_int expectedSeq = base_stage;//初始化期望收到报文的序列号
Packet recvPkt, sendPkt= makePacket(base_stage - 1);
```

`Server`端由于保证了窗口大小不变，对按顺序到达的数据包直接取下数据段复制到输出文件中；同时`Server`中缓存了一个答应按序达到最大序列号的`ACK`报文，如果收到一个错序的数据报，`Server`端直接抛弃并将这个缓存的`ACK`报文传输给`Client`.具体代码实现如下：

接收端由于不知道传输文件何时结束，所以时钟需要判断接收到的报文`flag`中`END`位是否被置为1，如果是1则说明文件传输完毕，接收端退出。

```c++
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
```

如果到达的报文是正常的数据报文且`recvPkt.head.seq==expectedSeq`，即是按序到达的报文，那么接收端将报文中的数据保存下来，并更新`sendPkt = makePacket(expectedSeq);expectedSeq=(expectedSeq+1)%MAX_SEQ;`。

为了实现累积重传，当接收端计时器没有启动，即第一次收到顺序到达的报文，那么就会启动计时器，并缓存下`sendPkt`，继续等待报文到达；如果超时`MAX_WAIT_TIME`，再关闭计时器并将`sendPkt`返回给发送端。此外，如果是一个不按顺序到达的报文，那么接收端直接重传缓存的`sendPkt`。

```c++
if(recvPkt.head.seq==expectedSeq && CheckPacketSum((u_short *) &recvPkt, sizeof(Packet)) == 0){
            //correctly receive the expected seq
            dataLen = recvPkt.head.bufSize;
            memcpy(fileBuffer + fileLen, recvPkt.data, dataLen);
            fileLen += dataLen;

            //give back ack=seq
            sendPkt = makePacket(expectedSeq);
            expectedSeq=(expectedSeq+1)%MAX_SEQ;
            //if this is the first expected seq
            if(!clockStart){
                start=clock();
                clockStart=true;
                continue;
            }else if(clock()-start>=MAX_WAIT_TIME){
                clockStart=false;
            }else continue;

            memcpy(pkt_buffer, &sendPkt, sizeof(Packet));
            sendto(socket, pkt_buffer, sizeof(Packet), 0, (SOCKADDR *) &addr, addrLen);
            continue;
        }
		clockStart=false；
        cout<<"[SYS]wait head:"<<expectedSeq<<endl;
        cout<<"[SYS]recv head:"<<recvPkt.head.seq<<endl;
        memcpy(pkt_buffer, &sendPkt, sizeof(Packet));
        sendto(socket, pkt_buffer, sizeof(Packet), 0, (SOCKADDR *) &addr, addrLen);
```



## 实验结果展示

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

<img src="F:\Computer_network\Computer_Network\Lab3\Lab3_2\img\image-20221125175727114.png" alt="image-20221125175727114" style="zoom:67%;" />

<img src="F:\Computer_network\Computer_Network\Lab3\Lab3_2\img\image-20221125175743183.png" alt="image-20221125175743183" style="zoom:67%;" />

**文件传输过程和四次挥手断开连接：**

<img src="Lab3_2\img\image-20221125180118262.png" alt="image-20221125180118262" style="zoom: 67%;" />

<img src="Lab3_2\img\image-20221125180147128.png" alt="image-20221125180147128" style="zoom:67%;" />

可见在第一次发送过程中发送端直接发送了`0~15`号数据报，并收到了对序列号为`10`报文的`ACK`报文，从而发生**窗口的滑动**，从原来的`[1,17]`滑动到`[11,27]`。

<img src="Lab3_2\img\image-20221125180432372.png" alt="image-20221125180432372" style="zoom:67%;" />

滑动结束后窗口中有11个空闲位置，所以发送端发送出`16~26`号数据报，此时该**窗口超时**未能收到`ACK`应答，将窗口内缓存的报文`[11~26]`全部重传一次，最后等到了`ACK=11`的应答报文，**窗口滑动**。

<img src="Lab3_2\img\image-20221125180606014.png" alt="image-20221125180606014" style="zoom:67%;" />

**对应接收端点的状态：**

<img src="Lab3_2\img\image-20221125181000484.png" alt="image-20221125181000484" style="zoom:67%;" />

序列号为`11`报文可能在传输过程发生丢包，没有按序到达，所以接收端始终在等候，抛弃其他错序到达的报文，最后等到重传时发送端传输过来的`11`号报文，接收端应答`ACK=11`。

**传输结果对比：**

<img src="Lab3_1\img\image-20221113192640466.png" alt="image-20221113192640466" style="zoom:67%;" />

![image-20221119133314159](Lab3_1\img\image-20221119133314159.png)

![image-20221119133336063](Lab3_1\img\image-20221119133336063.png)

![image-20221119133348934](Lab3_1\img\image-20221119133348934.png)

可见无论哪种类型的文件，传输前后都是一致的，验证了传输的可靠性。

## Lab3_2中的坑

一个编译环境的问题，由于我使用的环境是`Clion`，编译出来可执行文件内存非常小，所以如果还启用了多线程，那么很容易出现`new`报出`terminate called after throwing an instance of 'std::bad_alloc' what(): std::bad_alloc`这样的错误，为了解决只能在`CMakeList`里给可执行文件分配更多内存。

## GitHub仓库

[仓库链接](https://github.com/Stupid-wangnz/Computer_Network/tree/main/Lab3/Lab3_2)