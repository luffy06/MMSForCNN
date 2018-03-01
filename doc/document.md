# 应用模型

![数据流](./graph/dataflow.png)

![NoC](./graph/NoC.png)

通过输入的数据或是计算完的数据可以被存储vault（DRAM）和cache之中。众所周知，DRAM能够存储的数据远大于cache，但读取的速度缺慢了很多，所以**在存储数据的时候需要判断是存储在vault之中还是存储在cache之中**。由上一小节可知，PNG负责控制数据的流动，当计算开始时，PNG根据当前神经元所需要的数据去对应的存储位置取到数据，然后封装成对应的数据包，通过NoC网络传送给对应的PE进行计算。当PE中MAC计算完成后，将计算完成的结果传回给PNG，PNG判断后继需要这个数据的神经元，将其存储到对应PE的Cache之中，若Cache存储不下，则存储到对应的vault中。

时间消耗如下图所示。

![时间消耗](./graph/timecost.png)

# 算法

## 输入

**定义：**有向无环图DAG，$G=(V,E,P,R)$，其中$V$表示节点集合，节点个数是$N$；$E$表示边集合，边个数是$M$；$P$表示PE个数；$R$表示图$G$的重定时次数。

**定义：**$I$表示图$G$需要循环的次数。

## 输出

* 图$G$循环$I$次所需要的总时间$T$ 

## 过程

### Step One：计算每次发射所需要的核数和图循环次数

考虑到图最大的并发度是有限的，所以可以结合多发射的思想，合理的利用PE，限定每次发射（*Launch*）需要的PE个数，并将总的循环次数均摊到每次发射之上，以达到更高效的并行性。

**定义：**图的最大并发度是$m$，$H=\lceil P/m\rceil$。

那么图共需要$H$次发射。

若$m$整除$P$，

* 每次发射均需要的核数是$m$。
* 每次发射最多需要$I_{max}=\lceil I/H\rceil$次循环。

若$m$不整除$P$，

* 前$H-1$次发射的核数是$m$，最后一次发射核数是$P\%m$。
* 若前$H-1$次发射每次循环$x$次，最后一次发射循环$y$次。故$(x,y)$需要满足$I=(H-1)\times x+y$。对所有满足的$(x, y)$选取使总时间$T$最小的一组$(x,y)$。

### Step Two：确定单发射中每个周期内图循环的组数和每个核任务的种类

为了尽可能的减少PE空闲的时间，在每次发射中可以将不同周期的任务节点交错摆放在一起，以提高PE的利用率。

**定义：**一个周期（*Period*）为图$G$中节点集合$V$重复$I_{period}$次摆放在$m$个PE上的过程。

一个周期内$I_{period}$次节点集合$V$在$m$个PE上的摆放时在不考虑节点之间的依赖关系，尽可能的有效的利用PE。

因为图$G$中包含一些执行时间远大于其他的任务节点，若直接按照拓扑序列把所有任务节点依次插入各个PE，最终消耗的总时间会$T$很大。如图

![按拓扑序列排列](./graph/TopologyArrange.png)

主要的原因是按照拓扑序列排列时没有考虑到节点执行时间的特殊性，造成了分配的不均衡。本算法考虑这些长执行时间的任务节点所带来的影响，采取贪心的策略对任务节点进行排列。这个问题可以抽象成最小化最大值问题，即最小化PE上所有任务时间之和的最大值。算法的思想是对所有的节点按照执行时间大小从大到小排序，每次将枚举的节点放在当前所有PE中总时间最小的PE上。

![算法排列](./graph/OurArrange.png)

**定义：**$Ratio_{period}=\frac{I_{period}\times \sum_{V_{i}\in V}ex_{i}}{m\times ed^{PE}_{max}}$，其中$ed^{PE}_{max}$表示$m$个PE中PE上所有任务时间之和的最大值，$ex_{i}$表示任务节点$V_{i}$的执行时间。$Ratio_{limited}$表示周期的最低利用率。$I_{limited}$表示周期的最大循环次数。

$I_{period}$的值依据$Ratio_{period}$来确定。每按照上述算法排列一次图$G$，$I_{period}=I_{period}+1$，并重新计算一下当前的利用率$Ratio_{period}$，若$Ratio_{period}\ge Ratio_{limited}$，则停止排列；若$I_{perioid}\ge I_{limited}$，则停止排列，选择之前计算得到的最大的$Ratio_{period}$。

> $GenerateArrange():$
>
> $I_{period}\leftarrow 0,\; Ratio_{period}\leftarrow 0,\;Ratio_{periodmax}\leftarrow 0,\; I_{periodmax}\leftarrow 0,\;ed^{PE}_{max}\leftarrow 0$
>
> $sort(V) first\; by \;e_i\;,second\; by\; tp_{i}$
>
> $cost\leftarrow 0$
>
> $For\; i\leftarrow 0\; to \;N:$
>
> $\quad cost\leftarrow cost+ex_{i}$
>
> $For\;i\leftarrow 0\;to\;m:$
>
> $\quad ed^{PE}_{i}\leftarrow 0$
>
> $\quad ENQUEUE(Q_{prior}, ed^{PE}_{i})$
>
> $While\; Ratio_{period}\lt Ratio_{limited}\;and\;I_{period}\lt I_{limited}:$
>
> $\quad I_{period}\leftarrow I_{period}+1$
>
> $\quad For\;i\leftarrow 0\;to\;N:$
>
> $\quad\quad ed^{PE}_{j}\leftarrow DEQUEUE(Q_{prior})$
>
> $\quad\quad ed^{PE}_{j}\leftarrow ed^{PE}_{j}+ex_{i}$
>
> $\quad\quad ed^{PE}_{max}\leftarrow max(ed^{PE}_{max}, ed^{PE}_{j})$
>
> $\quad\quad ENQUEUE(Q_{prior},ed^{PE}_{j})$
>
> $\quad Ratio_{period}\leftarrow \frac{I_{period}\times cost}{m\times ed^{PE}_{max}}$
>
> $\quad If\; Ratio_{period}\gt Ratio_{periodmax}:$
>
> $\quad\quad I_{periodmax}\leftarrow I_{period}$
>
> $\quad\quad Ratio_{periodmax}\leftarrow Ratio_{period}$
>
> $If\;Ratio_{period}\lt Ratio_{limited}:$
>
> $\quad I_{period}\leftarrow I_{periodmax}$
>
> $\quad Ratio_{period}\leftarrow Ratio_{periodmax}$
>
> $Return\; I_{period}$

### Step Three：判断是否需要Retiming

![简单排列]()

**定义：**$T_{normal}$是按照上图排列所需要花费的总时间；$T_{estimate}$是当前算法的估计时间。

若$T_{estimate}\gt T_{normal}$，则按照上图排列。

计算$T_{estimate}$需要按照拓扑序列从前往后枚举每条边$e_{ij}$，

### Step Four：获取关键节点集合

**定义：**$S_{KeyNode}$为关键节点集合；参数$\alpha$。

对任务节点$V_i \in V$，若$e_i$满足$\frac{e_i}{e_{max}}\ge \alpha$，则认为$V_i$是关键节点，加入$S_{KeyNode}$。

$S_{KeyNode}$是一棵平衡二叉排序树，按照任务节点时间大小，从大到小进行排序。

> $GetKeyNodeSet(\alpha):$
>
> $e_{max}\leftarrow -\infty$
>
> $S_{KeyNode}\leftarrow \emptyset$
>
> $For \;i\leftarrow 0\;to\;N:$
>
> $\quad e_{max}\leftarrow max(e_{max}, e_i)$
>
> $For\;i\leftarrow 0 \;to\;N:$
>
> $\quad If\;e_i\ge e_{max}\times \alpha :$
>
> $\quad\quad ENSET(S_{KeyNode}, V_i)$
>
> $Return\;S_{KeyNode}$

### Step Five：从关键节点开始扩散，确定周期内所有节点的位置

考虑到图$G$中有一些执行时间远大于其他的任务节点，他们在PE上的排列将影响整个图$G$的重定时次数$R$，所以需要先确定他们的位置。

![扩散过程]()

利用第四步所获取的关键节点集合$S_{KeyNode}$，对其中的节点来确定其前继和后继节点的位置。当$S_{KeyNode}$为空后，从未访问的节点集合$S_{UnCheckedNode}$中再次获取关键节点集合。

>$SpreadFromKeyNode(S_{UnCheckedNode},\alpha)$
>
>$Q_{certain}\leftarrow \emptyset,\;Q_{uncertain}\leftarrow \emptyset$
>
>$S_{KeyNode}\leftarrow GetKeyNodeSet(\alpha)$
>
>$For\;i\leftarrow 0\;to\; N_{KeyNodeSet}:$
>
>$\quad\quad For\;r\leftarrow 1\;to\; I_{period}:$
>
>$\quad\quad\quad ArrangeKeyNode(V^r_i, Q_{certain}, Q_{uncertain})$
>
>$While\;Q_{uncertain}\neq \emptyset:$
>
>$\quad V^r_{i}\leftarrow DEQUEUE(Q_{certain})$
>
>$\quad If\;V^r_i\;hasn't\;been\;visited:$
>
>$\quad\quad ArrangeKeyNode(V^r_i, Q_{certain},Q_{uncertain})$
>
>$Return\;Q_{uncertain}$

### Step Six：确定关键节点$V^r_i$的位置，通过其入度（出度）边，利用动态规划确定入度（出度）边上传输的数据存储的位置（cache/DRAM），同时确定边所连接的节点的位置

在第二步中仅仅确定了每个PE中安排的任务节点都有哪些，但每个任务节点位置都是可以相互交换的。

**定义：**核为$p$的PE共有$K$个空闲的区间可以用来排列关键节点，第$k$个空闲区间的开始时间是$st^{Int}_{k}$，结束时间是$ed^{Int}_{k}$。一个周期内第$r$个的任务节点$V^r_i$所属于的PE核是$p^r_i$。

初始，每个PE上都有一个空闲区间$[st^{Int}_{0}, ed^{Int}_{0}]$，其中$st^{Int}_{0}\leftarrow 0$，$ed^{Int}_{0}\leftarrow T_{period}$。每个PE上的任务节点都属于自己PE的空闲区间。

接下来分为三个步骤：

1. 确定节点$V^r_i$的位置。

   若它属于它PE上的第$k$个区间，将其放在它该区间的最前面，即$st_i\leftarrow st^{Int}_{k}$，$ed_i\leftarrow st^{Int}_k+ex_i$。

2. 确定$V^r_i$的入度边上的数据存储位置及对应的节点。

   ![多个前继选择最近的]()

   考虑入度边$e_{ji}$，即$V_j$到$V_i$的边。从周期内未被确定的$U$个$V^u_j$节点，选择离$V^r_i$最近的一个，作为$V^r_i$的前继节点。

   假设$d_{ji}$可以放入Cache中，那么$V_j$的最晚开始时间为$st^{late}_j\leftarrow st_i - t^{Cache}_{ij}-ex_j$。对于$V^u_j$，在满足$st^u_j + R^u_j\times I_{period}\le st^{late}_j$的条件下，选取使得$st^u_j$最大的位置作为$V^u_j$离$V^r_i$最近的位置。 在$U$个已经选取最大位置的$V^u_j$中选择到距离$V^r_i$最近的任务节点$V^u_j$作为$V^r_i$的前继节点。

   因为Local Cache的容量有限，对于$V_i$的所有入度边上传输的数据需要**有选择**的放入Local Cache中。本算法采用动态规划的方法，尽可能的有效利用Local Cache。

   >$ArrangeInFixedSize(E_i, Size_{left}):$
   >
   >$S_{arranged}\leftarrow \emptyset$
   >
   >$For\;j\leftarrow 1\;to\;N_{E_i}:$
   >
   >$\quad For\;s\leftarrow \;Size_{left}\;to\;d_{ij}:$
   >
   >$\quad\quad dp[j][s]=max(dp[j-1][s-d_{ij}]+d_{ij},dp[j][s])$
   >
   >$s\leftarrow Size_{left}$
   >
   >$For\;j\leftarrow N_{E_i}to\;1:$
   >
   >$\quad If \;s\ge d_{ij}\;and\;dp[j-1][s-d_{ij}]+d_{ij}\gt dp[j][s]:$
   >
   >$\quad\quad ENSET(S_{arranged},e_{ij})$
   >
   >$Return S_{arranged}$

3. 确定与$V^r_i$同一个核的出度边上的数据存储位置及对应的节点。

   ![多个后继选择同核的]()

   本步与入度边时的处理基本相同，在处理之前，需要去除不是和$V^r_i$在同一个PE的任务节点$V^u_j$及他们之间的边$e_{ij}$。

> $ArrangeKeyNode(V^r_i, Q_{certain}, Q_{uncertain}):$
>
> $If\;V^r_i\;is\;not\;certain:$
>
> $\quad st_i\leftarrow st^{Int}_k,ed_i\leftarrow st^{Int}_k+ex_i$
>
> $S_{arranged}\leftarrow ArrangeInFixedSize(E^{In}_i, Size_{cache}-Size_{max})$
>
> $ENQUEUE(Q_{uncertain}, E^{In}_i-S_{arranged})$
>
> $For\;j\leftarrow 0\;to\;N_{S_{arranged}}:$
>
> $\quad ENQUEUE(Q_{certain}, V^u_j)$
>
> $\quad Update(ed^u_j,st^r_i,d_{ij})$
>
> $E^{Out}_i\;remove\;those\;edges\;e_{ij}\;that\;p_j\neq p_i$
>
> $S_{arranged}\leftarrow ArrangeInFixedSize(E^{Out}_i, Size_{cache}-Size_{max})$
>
> $ENQUEUE(Q_{uncertain}, E^{Out}_i-S_{arranged})$
>
> $For\;j\leftarrow 0\;to\;N_{S_{arranged}}:$
>
> $\quad ENQUEUE(Q_{certain}, V^u_j)$
>
> $\quad Update(ed^u_j,st^r_i,d_{ij})$

### Step Seven：确定剩余放在DRAM中的节点位置

在第六步中，未能放入cache中的任务节点需要放入DRAM中，这些节点的位置也是暂不确定的。不能确定的原因是当边$e_{ij}​$不能放入cache中时，节点$V^r_i​$的位置可能由另外一条能放入cache中的边$e_{ik}​$决定。



# 变量定义

| 变量                                           | 符号                         |
| ---------------------------------------------- | ---------------------------- |
| 图                                             | $G$                          |
| 节点集合                                       | $V$                          |
| 节点个数                                       | $N$                          |
| 拓扑序列、开始时间、执行时间、结束时间、所在核 | $tp_i, st_i,ex_i,ed_i,p_i$   |
| 边集合                                         | $E$                          |
| 节点$V_i$的入度边集合、出度边集合              | $E^{In}_i, E^{Out}_i$        |
| 边个数                                         | $M$                          |
| 从$V_i$到$V_j$的边，数据传输量，边的传输时间   | $e_{ij}, d_{ij},t_{ij}$      |
| PE总个数                                       | $P$                          |
| 图$G$需要循环的次数                            | $I$                          |
| 图$G$完成的总时间                              | $T$                          |
| 图$G$的最大并发度                              | $m$                          |
| 多发射次数                                     | $H$                          |
| 当$m\%P\not=0$，前$H-1$次发射循环的次数        | $x$                          |
| 当$m\%P\not=0$，最后一次发射循环的次数         | $y$                          |
| 利用率                                         | $Ratio$                      |
| 队列                                           | $Q$                          |
| 关键节点判断参数                               | $\alpha$                     |
| 关键节点集合                                   | $S_{KeyNode}$                |
| 核为$p_i$的PE上的第$k$个空闲区间               | $Int^{p_i}_k$                |
| 核为$p_i$的PE上的空闲区间个数                  | $K$                          |
| 第$k$个空闲区间的开始时间、结束时间            | $st_k^{Int}, ed_k^{Int}$     |
| 已经安排在Cache中的节点集合、未安排的          | $ArrangedSet, UnArrangedSet$ |
| $ArrangedSet$或$UnArrangedSet$中节点的个数     | $D$                          |
| $ArrangedSet$和$UnArrangedSet$中节点的下表     | $index_d$                    |
| 未检查节点集合                                 | $UnCheckedNodeSet$           |


