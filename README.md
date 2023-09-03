# 设计方案
实现的是一个锁管理器，提供加锁解锁功能，同时提供检测死锁功能，出现死锁后释放部分资源来解决死锁。

死锁的检测是通过检测死锁图中有没有环来实现的，如果对于请求同一资源的两个锁L1和L2(其对应的进程为P1和P2)，L1已经获得资源而L2在等待，则死锁图中有一条边P2->P1。

使用Korasaju算法（![参考](https://www.cnblogs.com/RioTian/p/14026585.html) ）找到图中所有的强联通分量。

死锁图一般是比较稀疏的图，存储使用邻接表。

设计时主要考虑的方面有：
1. 锁的实现
2. 死锁检测方法
3. victim选择

锁的数据结构为：
```c++
class Lock{
public:
    Lock(int p, int res, int stat=0);
    int node_name; // 所属进程
    int param_name; // 资源id
    int state; //0 == locked, 1 == waiting
};
```

锁的状态有两种，1. 已持有 2. 等待。

对于同一个资源加的锁放在链表中，方便检索和随机位置的删除。如果一个锁L1是资源R1对应链表的头，则他是一个已经持有的锁，链表其他位置的锁Ln都在阻塞等待L1释放，因此在死锁图中新建 Ln.node_name -> L1.node_name 边。

# V0 不考虑锁释放

这个版本先完成了基本功能，对于按照顺序请求锁的节点判断是否会形成死锁（形成环），如果形成死锁将相应的强连通图输出。
功能如下所示:

```
// 加锁
// lock writeNodeA param1
// lock writeNodeB param2
// lock writeNodeA param2
// lock writeNodeB param1

detected deadlock: writeNodeA->readNodeB->writeNodeA
***Can release node_name=writeNodeA(1) to break deadlock.***
detected deadlock: writeNodeA->readNodeB->writeNodeA
***Can release node_name=writeNodeA(1) to break deadlock.***
```

# V1 考虑释放锁

允许锁释放之后，释放一个锁需要对资源的锁列表进行修改，同时删除死锁图中对应的边。释放锁L0时，假设下一个获得锁的是L1，则在死锁图中删除Ln->L0，添加Ln->L1

释放一个锁之后，可能会产生新的死锁，如下例(`lock n1 p2`表示节点1请求参数2的锁)
```
lock w1 p2
lock w1 p3
lock w2 p2
lock w3 p3
lock w2 p3
lock w3 p2
release w1 p2
release w1 p3
```

死锁之后，要释放一部分锁，释放之后可能处于新的死锁状态，即可能有链式反应，所以释放锁之后中需要循环检测死锁并释放死锁节点，直到没有死锁为止。

被释放的节点选择为：循环等待中持有锁最少的节点。


为了进一步提升加锁解锁性能，需要将死锁检测放在后台线程中，每隔指定时间检测一次，不给加锁解锁增加额外开销。

检测线程也在LockManager对象的生命周期中进行管理，析构或者手动调用可以停止后台死锁检测。

功能如下所示：
```
lock n1 p1
set lock(node_name=n1, res_id=p1)
lock n2 p2
set lock(node_name=n2, res_id=p2)
lock n1 p2
set lock(node_name=n1, res_id=p2)
lock n2 p1
set lock(node_name=n2, res_id=p1)
detected deadlock: n2->n1->n2
***Can release node_name=n2(1) to break deadlock.***
release lock(node_name=n2, param_name=p2, state=0)
remove edge(n1->n2)
release lock(node_name=n2, param_name=p1, state=1)
erase node_name n2
```
