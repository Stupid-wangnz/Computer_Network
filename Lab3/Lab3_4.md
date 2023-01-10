# 性能对比实验

## 实验探究内容：

1. 停等机制与滑动窗口性能对比；
2. 滑动窗口机制不同窗口大小对性能的影响；
3. 有拥塞控制和无拥塞控制的性能对比。

## 实验设计思路

1. 为了保证实验数据的可靠性，每次测量程序运行时间取五次运行时间的平均值；
2. 使用`C++`中的`chrono`计时函数，更加准确，并且在测试性能时取消日志输出，增加实验结果准确性；
3. 采用控制变量的方法，分别测试传输文件大小、路由器丢包率、路由器延迟三个变量对程序的性能影响。

## 实验结果

**（1）停等机制与滑动窗口性能的对比**

传输时延的对比：

![image-20221229164118337](Lab3_4\image-20221229164118337.png)

吞吐量的对比：

![image-20221229164742591](Lab3_4\image-20221229164742591.png)

作出传输时延曲线图：

<img src="F:\Computer_network\Computer_Network\Lab3\Lab3_4\image-20221229165432334.png" alt="image-20221229165432334" style="zoom:80%;" />

估计丢包率，改变路由器延迟时间再做一组对比实验：

![image-20221229165605898](F:\Computer_network\Computer_Network\Lab3\Lab3_4\image-20221229165605898.png)

<img src="F:\Computer_network\Computer_Network\Lab3\Lab3_4\image-20221229172355636.png" alt="image-20221229172355636" style="zoom:80%;" />

最后对比不同文件大小两种传输机制的传输时延：

![image-20221229171938711](F:\Computer_network\Computer_Network\Lab3\Lab3_4\image-20221229171938711.png)



​	**综上可以总结停等机制和滑动窗口机制的性能差异：**

**总体而言滑动窗口的传输性能比停等机制更好，因为其可以一次性发送多条信息而减少了等待接收端回复ACK的时间，尤其随着路由器延时的增加RTT增加，滑动窗口的性能会明显优于停等机制。而丢包率的增加都会造成停等机制与滑动窗口性能的下降，但是两者始终性能差距不大，甚至滑动窗口有时性能不如停等机制。而随着传输文件大小增大，滑动窗口的性能优势更加明显。**



**（2）滑动窗口机制不同窗口大小对性能的影响**

对比窗口大小分别为8×MSS和16×MSS时的文件传输性能：

![image-20221229173306972](F:\Computer_network\Computer_Network\Lab3\Lab3_4\image-20221229173306972.png)

![image-20221229173651792](F:\Computer_network\Computer_Network\Lab3\Lab3_4\image-20221229173651792.png)



<img src="F:\Computer_network\Computer_Network\Lab3\Lab3_4\image-20221229175238462.png" alt="image-20221229175238462" style="zoom:80%;" />

<img src="F:\Computer_network\Computer_Network\Lab3\Lab3_4\image-20221229175931167.png" alt="image-20221229175931167" style="zoom:80%;" />

<img src="F:\Computer_network\Computer_Network\Lab3\Lab3_4\image-20221229175347165.png" alt="image-20221229175347165" style="zoom:80%;" />

<img src="F:\Computer_network\Computer_Network\Lab3\Lab3_4\image-20221229175812047.png" alt="image-20221229175812047" style="zoom:80%;" />



​	**可以明显对比出来，当窗口大小为8×MSS时传输性能明显优于16×MSS，说明并不是窗口越大传输性能效率越快，经过探究，发现在窗口大小为16*MSS时丢包率远大于路由器设定的丢包率，根本原因发送端一次性发送太多内容，远超过接收端的处理能力或者甚至超出UDP的缓冲区，导致频繁丢包，传输性能下降。由此可见，选择合理的窗口大小在滑动窗口机制里具有重要意义。**



**（3）有拥塞控制和无拥塞控制的性能对比**

![image-20221229201104624](F:\Computer_network\Computer_Network\Lab3\Lab3_4\image-20221229201104624.png)



<img src="F:\Computer_network\Computer_Network\Lab3\Lab3_4\image-20221229201252564.png" alt="image-20221229201252564" style="zoom:80%;" />

<img src="F:\Computer_network\Computer_Network\Lab3\Lab3_4\image-20221229201700031.png" alt="image-20221229201700031" style="zoom:80%;" />

从上图可以发现，在网络状况比较好（路由器延迟为0ms）的条件下，RENO拥塞控制算法下的程序性能不如无拥塞控制，因为RENO算法造成额外的时间消耗且随着丢包率增大窗口变化频率也增大，性能下降。

![image-20221229201921724](F:\Computer_network\Computer_Network\Lab3\Lab3_4\image-20221229201921724.png)

<img src="F:\Computer_network\Computer_Network\Lab3\Lab3_4\image-20221229202031553.png" alt="image-20221229202031553" style="zoom:80%;" />

<img src="F:\Computer_network\Computer_Network\Lab3\Lab3_4\image-20221229202047372.png" alt="image-20221229202047372" style="zoom:80%;" />

但是通过变化路由器延时可以发现，当网络状态变差，RENO拥塞控制算法下的程序运行性能随路由器延时增加下降得并不明显，总体曲线比较平稳。推测主要原因是快速重传机制的引入减少等待超时重传，且RENO算法顺利将滑动窗口设置为适合当前网络状况的大小。