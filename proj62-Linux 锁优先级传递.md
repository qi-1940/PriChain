![14.彩色中英文左右](.\img\14.彩色中英文左右.jpg)




增量开发说明要说明

做ppt讲解搭配视频配字幕：功能一二三序号:+名字测试（标注已完成），创新一二三，录视频需要停顿进行展示

红黑树结构体那一块链表参考文档记得贴

设计数据结构

第三第四阶段和项目总体方案要根据实际完成情况改

创新点和rbmutex锁实现细节处的扩展成员各自完成

情况说明需要改，根据参赛文档

视频测试结果（时延区别）

项目仓库文件树

目录

# proj62-Linux 锁优先级传递

* 赛题：`Linux 锁优先级传递`

- 队名：`PriChain`
- 成员：`钟家意 汪文琦 慕欣杭`
- 学校：`华中科技大学`

## 1. 目录

[TOC]

## 2. 目标描述与完成情况

### 2.1 目标描述

- 实现rwlock、rwsem、mutex的优先级继承支持
- 通过内核宏开关优先级继承相关特性

**具体内容**包括：

1. 完成OpenEuler安装与配置，更换4.19.325内核
2. 设计并实现支持优先级继承的内核互斥锁，保证高优先级线程等待低优先级线程持有锁时，能自动提升持有者优先级，防止优先级反转
3. 基于互斥锁，完成支持优先级继承的读写锁，实现多读单写场景下高优先级写者能提升所有持有读锁线程的优先级
4. 基于互斥锁，完成支持优先级继承的读写信号量，实现多读单写场景下高优先级写者能提升所有持有读信号量线程的优先级
5. 在加锁操作中集成死锁检测和递归加锁防护，检测到死锁或递归加锁时返回错误码并输出详细日志
6. 在解锁操作中检测未配对解锁，非持有者解锁时输出警告日志并拒绝解锁操作
7. 所有加锁/解锁接口返回错误码，便于上层感知和处理各种异常情况
8. 通过内核宏开关灵活启用或关闭优先级继承特性，支持性能对比和按需部署
9. 编写详细的内核测试用例，验证优先级继承、死锁检测和健壮性，日志输出直观展示优先级变化过程
10. 记录开发过程中的设计思路、技术难点和心得体会

基于以上内容，对项目目标进行拆分，我们需要在项目中完成以下功能：

| 目标编号 |                           目标内容                           |
| :------: | :----------------------------------------------------------: |
|    1     | 设计并实现支持优先级继承的内核互斥锁（rb_mutex），包括数据结构与基本接口 |
|    2     | 在rb_mutex中集成递归优先级继承机制，确保锁链上的优先级正确传递 |
|    3     |    实现rb_mutex的死锁检测功能，支持递归检测和详细日志输出    |
|    4     |  设计并实现基于rb_mutex的读写锁（rb_rwmutex），支持多读单写  |
|    5     | 完善rb_mutex和rb_rwmutex的内核模块导出接口，便于外部模块调用 |
|    6     | 编写详细的内核日志，记录锁操作、优先级继承、死锁检测等关键事件 |
|    7     | 设计并实现互斥锁和读写锁的单元测试用例，覆盖优先级继承和死锁场景 |
|    8     | 编写Makefile，实现rb_mutex、rb_rwmutex及测试模块的自动化编译与依赖管理 |
|    9     |  优化锁的性能，减少锁竞争和内存分配开销，提升内核模块稳定性  |
|    10    | 撰写项目技术文档，详细说明锁机制设计、优先级继承、死锁检测等实现原理 |
|    11    | 总结测试结果，分析优先级继承和死锁检测在实际场景下的表现与改进空间 |
|    12    | 梳理开发流程与经验，整理到Github仓库，便于开源发布和团队协作 |

### 2.2 完成情况（初赛）

| 预期功能编号 | 完成情况 |                         预期功能说明                         |
| :----------: | -------- | :----------------------------------------------------------: |
|      1       | 已完成   | 通过分析Linux内核互斥锁原理，设计了rb_mutex结构体，采用红黑树管理等待队列，实现了支持优先级继承的互斥锁。 |
|      2       | 已完成   | 在实现过程中发现简单优先级提升无法递归传递，最终采用递归遍历锁链的方式，确保优先级沿锁链正确继承，解决了优先级反转问题。 |
|      3       | 已完成   | 死锁检测初期只检测直接环，后通过递归遍历owner->waiting_on链，检测复杂死锁环路，并增加详细日志辅助调试。 |
|      4       | 已完成   | 读写锁初版存在写锁饥饿，后采用“第一个读者加写锁、最后一个读者释放写锁”策略，基于rb_mutex实现了公平的rb_rwmutex。 |
|      5       | 已完成   | 遇到模块间符号无法导出的问题，通过EXPORT_SYMBOL导出接口，并调整Makefile依赖，保证模块可独立编译和调用。 |
|      6       | 已完成   | 日志系统最初信息不足，后增加锁操作、优先级变化、死锁检测等详细日志，便于定位和复现问题。 |
|      7       | 已完成   | 测试用例初期覆盖不全，后补充了优先级继承、死锁检测、递归锁等极端场景，确保功能健壮性。 |
|      8       | 已完成   | Makefile最初未处理模块依赖，后细化编译规则，实现rb_mutex、rb_rwmutex及测试模块的自动化编译和依赖管理。 |
|      9       | 已完成   | 性能测试发现锁竞争时延较高，通过减少内存分配、优化临界区，显著提升了锁的性能和稳定性。 |
|      10      | 已完成   | 文档初稿仅有接口说明，后补充了设计原理、优先级继承机制、死锁检测流程和典型日志示例，便于他人理解和复用。 |
|      11      | 已完成   | 测试结果分析发现部分场景下优先级继承链过长，提出了优化建议，并总结了死锁检测的适用边界。 |
|      12      | 已完成   | 将开发流程、遇到的问题及解决方案整理到Github后续也整理到了Gitlab，便于团队交流和后续开源发布。 |

### 2.3 后续目标

1. 支持递归锁/可重入锁：扩展 rb_mutex，使同一线程可多次加锁，提升灵活性。
2. 性能与统计分析：增加锁竞争、死锁次数等运行时统计，便于性能调优和问题定位。
3. 多核优化：针对SMP环境优化自旋和唤醒策略，提升多核并发性能。
4. 丰富测试与可视化：补充极端并发/嵌套/优先级变化等测试场景，并考虑开发可视化工具展示锁依赖和死锁链路。
5. 开源推广与社区反馈：整理文档、发布开源，收集实际应用反馈，持续优化实现。



## 3. 方案设计与实现

### 3.1 基础优先级继承互斥锁rbmutex的结构体设计思路

- rb_mutex：

​	互斥锁的核心结构，记录当前持有者、等待队列（红黑树，按优先级排序）、自旋锁保护内部状态，以及等待队列用于阻塞/唤醒

- rb_mutex_waiter：

​	表示一个等待该锁的任务，包含任务指针、优先级、红黑树节点（用于插入等待队列）和链表节点

- rb_held_lock：

​	记录某个任务持有的锁及其最高等待者优先级，用于递归优先级继承和死锁检测

- rb_task_info：

​	维护每个任务的锁相关信息，包括正在等待的锁、原始优先级、持有的所有锁集合（红黑树），以及用于全局哈希表的节点

### 3.2 项目总体方案

​	本项目旨在为Linux内核实现优先级继承链机制，解决内核锁的优先级翻转问题，并通过内核宏开关灵活控制相关特性

#### 3.2.1 模块开发设计思路

- 核心目标：设计通用的优先级继承链机制，保证实时系统的确定性和响应性

- 统一框架：分析并抽象mutex、rwmutex、rwsem等多种锁的共性，构建统一的优先级继承与链式传递框架，支持多层次锁依赖关系

- 可配置性：通过内核宏开关灵活启用/关闭优先级继承链特性，便于不同场景下的适配和调试

#### 3.2.2 测试与验证方法

- mutex优先级继承验证：运行`rb_mutex_user`模块，验证基本优先级继承功能

- rwmutex优先级继承验证：运行`rb_rwmutex_user`测试脚本，检验读写锁场景下的优先级传递

- rwsem优先级继承验证：使用`rb_rwsem_user`s模块进行压力测试，确保信号量场景下的继承链有效

- 内核宏开关配置：在.config文件中设置CONFIG_RT_PI_CHAIN选项，控制优先级继承链机制的启用

- 死锁检测验证：在`rb_mutex_user`中主动构造死锁场景，观察内核日志中死锁检测和链路追踪的详细输出





## 4. 开发计划与工作重要进展

**开发的流程参考标准内核开发流程**：

 

![开发流程图](.\img\开发流程图.png)

### 4.1 第一阶段 环境配置和内核更换

* #### 环境配置（基于Vmware Workstation 17.5及openEuler 20.04 SP4的环境配置）

  * ##### **ISO镜像文件下载**

    在官网下载openEuler 20.04 SP4的镜像文件

    https://www.openeuler.openatom.cn/zh/download/archive/detail/?version=openEuler%2020.03%20LTS%20SP4

    选取Standard版本进行安装

  * ##### **安装**

    安装链接参考这篇博客

    https://blog.csdn.net/2302_82189125/article/details/137759482

    注意选择linux版本建议选为 其他linux版本 4.x内核版本

  * ##### **配置网卡**

    * ###### **问题**

      在完成安装后使用yum指令进行相关的软件包下载，会提示无法解析域名

    * ###### **问题分析**

      尝试`ping 8.8.8.8`，失败 网络未正常连接

    * ###### **修复**

      关闭虚拟机，在vmware的虚拟机设置中确认虚拟机的相关设置是否正确

      ![image-20250624154847002](.\img\image-20250624154847002.png)

      确定为NAT链接，且启动时链接选项被勾选。

      在终端输入指令 `ip link show`

      终端回显

      ```shell
       ens33: <BROADCAST,MULTICAST> mtu 1500 qdisc noop state DOWN mode DEFAULT group default qlen 1000
      ```

      可以看到state为DOWN，若你的不是，跳过这步

      终端输入指令`sudo ip link set ens33 up`来打开网卡

      使用`ip a`查看是否分配到ip

      在网卡下如ens33下是否有`inet xxx.xxx.xxx.xxx`的ip显示

      若没有，则需要启用DHCP，输入`sudo dhclient ens33`。

      再次进行ping测试，若成功则修复完成

    * ###### ***（可选）*** 开启DHCP服务自动获取ip

      openEuler使用NetWorkManager管理网络

      使用`nmcli connection show`查看连接配置

      使用`nmcli connection modify "Wired connection 1" connection.autoconnect yes`开启该连接的自动连接

      使用`nmcli connection up "Wired connection 1"`启用网络连接

  * ##### 安装图形界面

    * ###### 此处安装DDE桌面环境

      ```shell
      yum install dde
      ```

    * ###### 设置以图形化方式启动系统

      ```shell
      systemctl set-default graphical.target
      ```

    * ###### 重启系统

      ```shell
      reboot
      ```

  * ##### 修改电源计划

    * ###### 修改电源熄屏时间

      ​	![image-20250624155420117](.\img\image-20250624155420117.png)

  * ##### 配置VSCODE ssh远程连接

    * ###### ifconfig查看ip信息

      在虚拟机的终端中输入指令`ifconfig`，若没有请安装`net-tools`软件包

      记录网卡的ip地址，即虚拟机的ip

      ![image-20250624155801123](.\img\image-20250624155801123.png)	

    * ###### 安装ssh服务并做注释修改

      在虚拟机上安装ssh服务`yum install openssh-server`

      打开`/etc/ssh/sshd_config`
      去掉这两条的注释
      ```c
      #Port 22
      ···
      #PubkeyAuthentication
      ```

    * ###### 重新启动ssh服务

      ```shell
      systemctl restart sshd
      ```

    * ###### powershell连接到虚拟机

      在主机的powershell中尝试用`ssh "username"@ipaddress`连接到虚拟机，出现以下终端画面表示成功

      ![image-20250624160121385](.\img\image-20250624160121385.png)	

    * ###### 配置vscode

      下载remote ssh插件，已下载则跳过，在远程连接选项中打开ssh的配置文件

      ![image-20250624160252197](.\img\image-20250624160252197.png)修改hostname，此处identifyfile先不填，在下一步配置ssh密钥中再添加

    * ###### 配置vscode报错

      ![image-20250624160317673](.\img\image-20250624160317673.png)

    * ###### 报错解决

      ```shell
      vim /etc/ssh/sshd_config
      ```

      去掉注释

      ```c
      #AllowTcpForwarding yes
      ```

      重启ssh服务

      ```shell
      systemctl restart sshd
      ```

    * ###### 连接VSCODE

      出现如下画面，即右上角出现install时，连接成功，点击加号打开新终端

      ![image-20250624160529816](.\img\image-20250624160529816.png)出现如下图片连接成功

      ![image-20250624160545676](.\img\image-20250624160545676.png)如果还遇到其他错误，则可以CTRL+SHIFT+U查看输出，进行报错解决	

    * ###### 配置免密登录

      在windows的powershell中输入`ssh -keygen -t rsa`,之后一路回车，生成密钥

      ![](.\img\image-20250624160710362.png)完成后将该目录下的`id_rsa.pub`中的内容复制到虚拟机的/root/.ssh目录下，建议在vscode的终端中进行该步骤操作，可直接复制

      ![](.\img\image-20250624160724152.png)	
      加入私钥路径，配置完成

* #### 4.19.325内核编译及安装

  * ##### 查看当前内核版本

    ![](.\img\image-20250624160951945.png)	

    为OE官方发布的内核，这里需要替换成linux标准内核

  * ##### 下载相关包

    ```shell
    yum install -y gcc gcc-c++ make cmake unzip zlib-devel libffi-devel openssl-devel pciutils net-tools sqlite-devel lapack-devel openblas-devel gcc-gfortran ncurses-devel bison m4 flex bc
    sudo dnf install elfutils-libelf-devel ncurses-devel openssl-devel bc flex bison perl wget git gcc make dracut
    ```

  * ##### 解压内核文件

    `tar -xvf linux-4.19.325.tar.xz`对内核文件进行解压

  * ##### 配置编译选项

    进入解压后的目录，输入`make menuconfig`进行编译配置

  * ##### 编译内核并构建

    ```shell
    make -j$(nproc) && make modules_install && make install
    
    update-grub  # 如果使用 grub
    
    reboot
    ```

    报错

    ```shell
    Cannot generate ORC metadata for CONFIG_UNWINDER_ORC=y, please install libelf-dev, libelf-devel or elfutils-libelf-devel
    ```

    解决

    ```shell
    sudo yum install elfutils-libelf-devel
    ```

    报错

    ```shell
    Error! Bad return status for module build on kernel: 4.19.325 (x86_64)
    
    Consult /var/lib/dkms/kmod-kvdo/6.2.2.24-10/build/make.log for more information
    ```

    这是 DKMS（动态内核模块支持）尝试为 kmod-kvdo 构建模块失败 所导致的错误，它 不会影响你正常安装和使用编译后的内核本身，但为了保持系统干净或避免未来警告，你可以按以下方法处理

    解决

    卸载kvdo

    ```shell
    sudo dnf remove kmod-kvdo kvdo
    ```

  * ##### 生成 initramfs 文件

    ```shell
    sudo dracut -f /boot/initramfs-4.19.325.img 4.19.325
    ```

  * ##### 更新grub引导项

    ```shell
    sudo grub2-mkconfig -o /boot/grub2/grub.cfg
    ```

  * ##### 重启虚拟机并验证

    ```shell
    sudo reboot
    uname -r
    ```

    ![](.\img\image-20250624161643708.png)	

### 4.2 第二阶段 调研分析

#### 4.2.1 内核锁优先级机制理论

##### 4.2.1.1 优先级翻转

* ###### 定义:

  优先级翻转是指**高优先级任务因等待低优先级任务占用的共享资源而被阻塞**，同时**中等优先级任务抢占低优先级任务**，导致高优先级任务无法及时执行的现象

* ###### **危害**：

  **资源竞争**：高优先级任务响应延迟，可能导致系统失效（如火星探路者号事故）

  **实时性破坏**：任务执行时间波动，难以满足硬实时要求，无法保证实时性

* ###### 示例介绍:

  ![](.\img\优先级翻转.png)

  以时间流动为基准，低优先级任务首先**占用共享资源**，直接导致高优先级任务在请求该资源时被**阻塞**而无法执行；此时中优先级任务因无需资源而**持续执行**，间接阻碍了低优先级任务释放资源的进程，最终造成本应优先执行的高优先级任务反而被延迟最久的不合理现象——严重破坏实时系统的任务调度时序

* ###### 解决方案：

  * **优先级继承（Priority Inheritance）**：

    当 **H** 等待 **L** 持有的资源时，**L 临时继承 H 的优先级**，避免被 **M** 抢占。资源释放后，**L** 恢复原优先级

  * **优先级天花板（Priority Ceiling）**：

    任务获取资源时，**直接提升至预设的最高优先级**（即“天花板优先级”），无论是否有高优先级任务等待。该方法简单但可能过度提升优先级

##### 4.2.1.2 优先级传递

* ###### 定义：

  优先级传递是**通过动态调整任务优先级，解决资源竞争引发的阻塞问题**，本质是优先级继承的实现方式。其核心是**将高优先级任务的优先级“传递”给低优先级任务**，确保资源持有者尽快执行

* ###### 工作流程:

  > H 请求被 L 占用的资源
  > 系统检测到阻塞，将 L 的优先级提升至与 H 相同
  > L 以高优先级运行直至释放资源
  > L 恢复原优先级，H 获取资源继续执行

* ###### 示例介绍：

  ![image-20250624174324909](.\img\image-20250624174324909.png)	

  **事件链条**

  ```markdown
  1. L持有B
  2. M持A→请求B(阻塞)
  3. H抢占M→请求A(阻塞)
  4. 一级继承：M继承H优先级
  5. M以H级请求B(阻塞)
  6. 二级继承：L继承M优先级(即H级)
  7. L以H级释放B
  8. M获得B→释放A
  9. H获得A→执行
  10. L恢复低优先级 | M恢复中优先级
  ```

* ###### **关键机制深度解析**

  * **多级优先级继承的动态传递**

    * 需要实现优先级继承链来作为通道传递优先级
      如`H阻塞在M持有的资源A→M继承H优先级→M阻塞在L持有的资源B→L继承M优先级（间接继承H）`
    * 优先级继承**可递归传递**，直至资源持有链的源头

  * **避免死锁与优先级翻转的保障**

    * **防死锁设计**：

      资源请求顺序为**单向依赖链**（M需A→B，H需A，L需B），无循环等待（如“H等A，M等B，L等A”）

    * **防翻转设计:**

      通过继承机制，资源持有者（L/M）**临时获得阻塞者的高优先级**，避免被中优先级任务抢占（若无继承，TaskM可能抢占TaskL导致H无限等待）

  * **恢复优先级的精准时机**

    * **释放资源后立即恢复**

      仅当低优先级任务持有高优先级任务所需资源时，才暂时提升其优先级
      一旦资源释放，阻塞链断裂，继承理由即消失。此时若延迟恢复优先级，将违反设计初衷
避免“伪高优先级”任务持续占用CPU；防止优先级错位引发二次翻转； 最小化高优先级传播范围；保障资源分配公平性

##### 4.2.1.3 优先级继承链

* ###### 定义:

  优先级继承链是实时系统为解决**嵌套阻塞型优先级翻转**而设计的动态优先级传播机制：
  **当高优先级任务因资源被低优先级任务占用而阻塞时，低优先级任务临时继承高优任务的优先级，使阻塞链的传递形成优先级“链条”**

* ###### 背景：

  在实时系统中，优先级翻转问题因资源竞争引发的高优先级任务阻塞而频繁发生。传统优先级继承机制虽能缓解单层资源阻塞（如低优先级任务持有锁时临时继承等待者的高优先级），但在**多锁嵌套、树状阻塞链或动态任务竞争**等复杂场景中存在明显局限。例如，当多个高优先级任务等待同一锁时，持有者优先级可能无法实时响应新增的高优等待者；若任务需嵌套获取多个锁（如任务C持有锁X后请求锁Y），继承机制难以覆盖多层依赖关系，导致阻塞链传递延迟。更严峻的是，非任务实体（如中断服务程序）占用的资源无法触发优先级继承，使得高优先级任务仍面临不可控的阻塞风险。这些缺陷在火星探测器Pathfinder系统故障等案例中暴露无遗，迫使开发者设计更健壮的解决方案——优先级继承链（PI Chain）

* ###### 作用：

  优先级继承链通过**动态构建阻塞关系的拓扑结构**，实现优先级沿资源依赖路径的递归传播，从根本上优化嵌套阻塞场景的实时性。其核心作用体现为三点：

  1. **树状优先级传播**：当任务形成多级阻塞链（如P5→P4→P3→P2→P1）或树形结构（如P6和P5同时阻塞于P2），PI Chain将链顶（最高优先级任务）的优先级**反向传播至所有下游持有者**。例如P2需继承直接阻塞者P3、P4、P6中的最高优先级，并递归传递至P1，确保整条依赖路径上的任务均以不低于链顶的优先级执行
  2. **消除中优先级干扰**：通过强制提升阻塞链中所有资源持有者的临时优先级，彻底避免中优先级任务抢占资源持有者（如图中TaskM抢占TaskL的情况），从而打破“高优任务被中优任务间接阻塞”的翻转逻辑
  3. **动态依赖维护**：内核通过**红黑树管理阻塞队列**，实时跟踪任务间的等待关系（如Linux的`pi_waiters`），确保新增阻塞任务时优先级继承立即更新。结合原子操作（如`cmpxchg`）实现无锁化优先级提升，最小化传播延迟，满足硬实时系统的死线约束

* ###### 示例介绍：

  ```markdown
       → L3 → P3
     ↗          ↘
  P5               L2 → P2 → L1 → P1
     ↘          ↗ ↑
       → L4 → P4  L5 ← P6
          
  ```

  链中任意进程 `P_k` 的临时优先级满足：
  $$
  P_k' = \min\left(P_k, \min_{\forall P_i \in \text{等待}} \left( \max(P_i') \right)\right)
  $$

  - 设原始优先级：
    - **P1**（持有 L1）：优先级 **50**
    - **P2**（持有 L2 和 L5）：优先级 **40**
    - **P3**（持有 L3）：优先级 **30**
    - **P4**（持有 L4）：优先级 **20**
    - **P5**（等待 L4）：优先级 **10**
    - **P6**（等待 L5）：优先级 **15**
  - 步骤:
    - **P5（优先级10）请求L4（被P4持有）→ 阻塞**
    - **触发第一级继承**：P4继承P5优先级 → `P4.prio = min(20, 10) = 10`
    - **P4（现优先级10）请求L3（被P3持有）→ 阻塞**
    - **触发第二级继承**：P3继承P4优先级 → `P3.prio = min(30, 10) = 10`
    - **P3（现优先级10）请求L2（被P2持有）→ 阻塞**
    - **触发第三级继承**：P2继承P3优先级 → `P2.prio = min(40, 10) = 10`
    - **P6（优先级15）请求L5（被P2持有）→ 阻塞**
    - **触发优先级重计算：**
      - P2继承所有阻塞者最高优先级 → `P2.prio = min(40, max(10, 15)) = 15`
    - **P2（现优先级15）请求L1（被P1持有）→ 阻塞**
    - **触发第四级继承**：P1继承P2优先级 → `P1.prio = min(50, 15) = 15`
    - **P1（现优先级15）执行临界区 → 释放L1**
    - **P1优先级恢复**：立即回退至原始值 50
    - **P2获取L1 → 执行临界区 → 释放L2**
    - **P2优先级重计算**：仅剩P6等待 → `P2.prio = min(40, 15) = 15`
    - **P3获取L2 → 执行临界区 → 释放L3**
    - **P3优先级恢复**：回退至原始值 30
    - **P4获取L3 → 执行临界区 → 释放L4**
    - **P4优先级恢复**：回退至原始值 20
    - **P5获取L4 → 执行任务 → 释放L4**
    - **P2（持有L5）执行临界区 → 释放L5**
    - **P2优先级恢复**：回退至原始值 40
    - **P6获取L5 → 执行任务 → 释放L5**

#### 4.2.2 内核锁优先级机制实现

##### 4.2.2.1 双红黑树的数据结构

* ######  红黑树在 rt_mutex 中的作用

  * 目的：高效管理所有等待同一把 rt_mutex 的任务（waiter），并且能快速找到优先级最高的等待者
  * 优势：红黑树是一种自平衡二叉查找树，插入、删除、查找的时间复杂度都是 O(logN)，比链表在大量等待者时更高效
  * 补充：链表参考文档

* ###### 相关结构体

  * **struct rb_root_cached**

    ```c
    struct rb_root_cached {
    
      struct rb_root rb_root;
    
      struct rb_node *rb_leftmost;
    
    };
    
    - rb_root：红黑树的根节点。
    
    - rb_leftmost：指向树中最左侧的节点（即优先级最高的 waiter），便于快速唤醒。
    ```

  * **struct rt_mutex**

    ```c
    struct rt_mutex {
    
      raw_spinlock_t   wait_lock;
    
      struct rb_root_cached waiters; *// 红黑树*
    
      struct task_struct *owner;
    
      *// ...*
    
    };
    ```

  * **struct rt_mutex_waiter**

    ```c
    struct rt_mutex_waiter {
    
      struct rb_node   tree_entry;   *// 用于插入到 rt_mutex 的 waiters 红黑树*
    
      struct rb_node   pi_tree_entry;  *// 用于插入到 owner task 的 pi_waiters 红黑树*
    
      struct task_struct *task;
    
      *// ...*
    
    };
    ```

* ###### 红黑树的插入/删除/查找流程

  *  **插入**

    - 当一个 task 需要等待某个 rt_mutex 时，会创建一个 rt_mutex_waiter
    - 该 waiter 的 tree_entry 作为节点，按优先级插入到 rt_mutex 的 waiters 红黑树
    - 插入时会维护红黑树的平衡性，并更新 rb_leftmost 指针

  * **删除**

    * 当锁 owner 释放锁时，需要唤醒优先级最高的 waiter
    * 通过 rb_leftmost 快速找到最左节点（最高优先级 waiter），将其从红黑树中删除

    * 删除后红黑树自动调整，rb_leftmost 也会被更新

  * **查找**

    * 查找优先级最高的 waiter 只需访问 rb_leftmost，时间复杂度 O(1)。

    * 其他查找操作（如按优先级查找某个 waiter）为 O(logN)。

      ![](C:\Users\27660\Desktop\文档\image-20250624192845349.png)	

  * ###### 红黑树结构体成员讲解

    * **struct rb_root / struct rb_root_cached**

      定义在 include/linux/rbtree.h：

      ```c
      struct rb_root {
      
        struct rb_node *rb_node;
      
      };
      
      struct rb_root_cached {
      
        struct rb_root rb_root;
      
        struct rb_node *rb_leftmost;
      
      };
      ```

      - rb_node：红黑树的根节点。

      - rb_leftmost：指向树中最左侧的节点（通常是优先级最高的 waiter），便于快速查找。

    * **struct rb_node**

      ```c
      struct rb_node {
      
        unsigned long __rb_parent_color;
      
        struct rb_node *rb_right;
      
        struct rb_node *rb_left;
      
      };
      ```

      - *rb_parent_color：父节点指针和颜色信息（红/黑），用位操作节省空间。*

      - rb_right：右子节点指针。

      - rb_left：左子节点指针。

      每个 rt_mutex_waiter 结构体中都有一个 rb_node 成员（如 tree_entry 或 pi_tree_entry），用于插入到红黑树中。

##### 4.2.2.2 rtmutex核心函数

* ###### rt_mutex_lock()

  ```c
  void __sched rt_mutex_lock(struct rt_mutex *lock)
  {
  	__rt_mutex_lock(lock, 0);
  }
  EXPORT_SYMBOL_GPL(rt_mutex_lock);
  
  static inline void __rt_mutex_lock(struct rt_mutex *lock, unsigned int subclass)
  {
  	rt_mutex_lock_state(lock, subclass, TASK_UNINTERRUPTIBLE);
  }
  
  static inline int __sched rt_mutex_lock_state(struct rt_mutex *lock,
  					      unsigned int subclass, int state)
  {
  	int ret;
  
  	mutex_acquire(&lock->dep_map, subclass, 0, _RET_IP_);
  	ret = __rt_mutex_lock_state(lock, state);
  	if (ret)
  		mutex_release(&lock->dep_map, 1, _RET_IP_);
  	return ret;
  }
  
  int __sched __rt_mutex_lock_state(struct rt_mutex *lock, int state)
  {
  	might_sleep();
  	return rt_mutex_fastlock(lock, state, NULL, rt_mutex_slowlock);
  }
  ```

  主流程：最终调用 rt_mutex_fastlock()，如果加锁失败则进入 rt_mutex_slowlock() 慢路径

* ###### rt_mutex_unlock()

  ```c
  static void
  rt_mutex_enqueue(struct rt_mutex *lock, struct rt_mutex_waiter *waiter)
  {
  	struct rb_node **link = &lock->waiters.rb_root.rb_node;
  	struct rb_node *parent = NULL;
  	struct rt_mutex_waiter *entry;
  	bool leftmost = true;
  
  	while (*link) {
  		parent = *link;
  		entry = rb_entry(parent, struct rt_mutex_waiter, tree_entry);
  		if (rt_mutex_waiter_less(waiter, entry)) {
  			link = &parent->rb_left;
  		} else {
  			link = &parent->rb_right;
  			leftmost = false;
  		}
  	}
  
  	rb_link_node(&waiter->tree_entry, parent, link);
  	rb_insert_color_cached(&waiter->tree_entry, &lock->waiters, leftmost);
  }
  
  static void
  rt_mutex_dequeue(struct rt_mutex *lock, struct rt_mutex_waiter *waiter)
  {
  	if (RB_EMPTY_NODE(&waiter->tree_entry))
  		return;
  
  	rb_erase_cached(&waiter->tree_entry, &lock->waiters);
  	RB_CLEAR_NODE(&waiter->tree_entry);
  }
  ```

  将 waiter 插入/移除到 rt_mutex 的等待队列（红黑树）

* ###### rt_mutex_enqueue() 和 rt_mutex_dequeue()

  ```c
  static int rt_mutex_adjust_prio_chain(struct task_struct *task,
  				      enum rtmutex_chainwalk chwalk,
  				      struct rt_mutex *orig_lock,
  				      struct rt_mutex *next_lock,
  				      struct rt_mutex_waiter *orig_waiter,
  				      struct task_struct *top_task)
  {
  	// ... 代码较长，已在前面详细展开 ...
  	// 递归遍历优先级继承链，提升 owner 及其上游 owner 的优先级
  }
  ```

  递归调整优先级继承链，处理多级优先级反转

* ###### rt_mutex_setprio()

  ```c
  static void rt_mutex_setprio(struct task_struct *p, struct task_struct *pi_task)
  {
  	// 主要用于设置任务的优先级，通常在优先级继承时调用
  	// 代码在 sched/core.c 或 sched/rt.c 中实现
  }
  ```

  设置任务的优先级，配合优先级继承

* ###### 与调度器、task_struct 的交互

  - 唤醒：rt_mutex_wake_waiter() 调用 wake_up_process() 唤醒等待者。

  - 优先级提升/恢复：rt_mutex_adjust_prio()、rt_mutex_setprio() 调用调度器相关接口，提升/恢复 owner 的优先级。

  - task_struct 关联：pi_waiters 记录所有等待该任务释放锁的 waiter，pi_blocked_on 指向当前任务正在等待的 waiter。

#### 4.2.3 分析现有mutex、rwmutex以及rwsem的锁或信号量特性

##### 4.2.3.1 mutex

* ###### **核心特性**

  * **独占访问**：同一时刻仅允许一个线程持有锁，严格互斥
  * **休眠机制**：竞争失败时线程进入睡眠状态，由内核调度器管理唤醒，避免忙等待
  * **优先级继承**：支持优先级继承，防止低优先级线程持有锁时导致高优先级线程阻塞（优先级反转问题）

* ###### **三级路径优化**

  * **快速路径**：无竞争时通过原子操作（`cmpxchg`）直接获取锁
  * **中速路径（乐观自旋）**：基于 MCS 锁实现自旋等待，减少上下文切换开销
  * **慢速路径**：竞争激烈时休眠并加入等待队列

##### 4.2.3.2 rwmutex

* ###### **核心特性**

  * **读写分离**：
    * **读锁（RLock）**：允许多个线程并发读，无互斥
    * **写锁（Lock）**：独占模式，阻塞所有读写操作

  * **公平性策略**：
    * **读者优先**：默认模式，可能引致写者饥饿
    * **写者优先**：Linux 4.16+ 支持 `RWSEM_WRITER_BIAS`，减少写者等待时间

* ###### **性能优化**

  * **乐观自旋（OSQ）**：
    * 读多写少时，读者通过 MCS 队列自旋等待，减少休眠
    * 写者自旋受限（因需等待所有读者退出）

  * **非自旋标记（NONSPINNABLE）**：
    * 读者过多或等待超时时标记锁为不可自旋，强制进入休眠路径

##### 4.2.3.3 rwsem

* ###### **与 RWMutex 的差异**

  * **信号量语义**：`rwsem` 是**可睡眠的读写锁**，而 `rwlock`（自旋锁）仅适用于短临界区

  * **数据结构**：

    ```c
    struct rw_semaphore {
        atomic_long_t count;    // 分字段：读者数 + 写者标志
        atomic_long_t owner;   // 写者持有时的任务指针
        raw_spinlock_t wait_lock;
        struct list_head wait_list;  // 等待队列
    }[2](@ref)。
        
    count 按位划分：低比特标识写者（RWSEM_FLAG_WAITERS），高位记录读者数量
    ```

  ###### **关键机制**

  * **批量唤醒**：释放读锁时，一次性唤醒等待队列中所有读者（即使队列中有写者插队）
  * **锁传递（Handoff）**：写者释放时若检测到等待队列，直接将锁传递给队首任务（通过 `MUTEX_FLAG_HANDOFF`）
  * **中断处理**：支持 `down_read_interruptible()`，允许睡眠期间响应信号


### 4.3 第三阶段 编写测试程序进行rbmutex的模块开发

说明：该部分在后续文档中有更详细的叙述，此处仅简要介绍

1. 完成数据结构定义与优化：设计 `struct rbmutex`，定义红黑树节点 `struct rb_waiter`（含任务指针、优先级、PI 状态）
2. 实现红黑树队列：实现等待任务插入/删除（按优先级排序，O(log n) 复杂度），惰性删除机制（节点延迟释放 + 内存复用）
3. 完成优先级继承集成：动态提升锁持有者优先级，使得在阻塞时继承最高等待任务优先级；递归传播 PI；释放锁时重新计算优先级
4. 完成加锁/解锁优化：快速路径：无竞争时通过原子操作（`cmpxchg`）直接获取锁；慢速路径：竞争时插入红黑树并触发阻塞；批量唤醒
5. 完成死锁预防与调试：扩展 `lockdep` 规则检测 PI 链死锁；实现 `CONFIG_DEBUG_RBMUTEX` 跟踪锁状态变更

### 4.4 第四阶段 扩展实现读写锁（信号量）并编写内核宏开关进行主线集成

说明：该部分在后续文档中有更详细的叙述，此处仅简要介绍

1. 完成原有基础上数据结构的扩展
2. 完成读锁优化实现：无写锁时通过 `atomic_inc(&count)` 直接获取读锁；竞争时插入红黑树，触发优先级继承传播至持有写锁的任务
3. 完成写锁阻塞机制：获取写锁时若存在读锁持有者，将写任务加入红黑树并触发 PI 机制提升当前读锁持有者的优先级，加速其释放
4. 优化锁释放与唤醒：释放写锁时，从红黑树中批量唤醒最高优先级读任务；读锁释放时，检查写等待者并唤醒首个写任务
5. 添加 Kconfig 开关：`CONFIG_RBMUTEX`

## 5. 仓库文件树概览

```plaintext
project2210132-239331
├── Final_Report
├── RAG_bge                 //检索增强文件配置
│   ├── BgeSearchModule.py
│   ├── bge_config.json
│   ├── bge_test_hit.py
│   └── how_to_finetune.md
├── RAG_evaluate            //检索增强模型评估
│   └── how_to_evaluate_retrieval.md
├── RAG_tfidf               //TF-IDF模型
│   ├── tfidf_config.json
│   ├── tfidf_hit.py
│   └── tfidf_train.py
├── README.md
├── UI_communication        //可视化界面代码编写
│   ├── ui.py
│   ├── ui_rag.py
│   └── ui_ver2.py
├── dataset                 //数据集
│   ├── output.jsonl
│   ├── output20000.csv
│   ├── output20000.jsonl
│   ├── processed_output_with_q.jsonl
│   └── processed_output_with_q_old.jsonl
├── dataset_build           //构建数据集
│   ├── BuildTrainDataModule.py
│   ├── build_dataset.py
│   ├── clean_to_chunks.py
│   ├── delete_pream.py
│   ├── filter_by_filetype.py
│   ├── how_to_build_dataset.md
│   ├── process_markdown.py
│   └── sample_build_config.json
├── dev_docs
│   ├── 00_Prepare.md
│   ├── 01_Research.md
│   └── 02_Methods.md
├── four_layer_test_hit.py
├── pgSQL
│   ├── bge_test_hit.py
│   └── save_data_to_postgre.py
├── pic
│   ├── Pro.png
│   ├── Process.png
│   ├── hit_acc.png
│   ├── system_workload1.png
│   ├── system_workload2.png
│   └── ustc.png
├── three_layer_test_hit.py
├── two_layer_test_hit.py
└── vectorize               //测试模型向量化加载
│   ├── desirial.py
│   └── vectorize.py
├── README.md               //实验文档
├── four_layer_test_hit.py  //嵌套多层检索的尝试
├── three_layer_test_hit.py
└── two_layer_test_hit.py
```





## 6. 优先级继承链的思路分析

1. 每个任务都维护自己的原始优先级和当前等待的mutex
2. 当高优先级任务请求mutex时，递归提升所有相关持有者的优先级，形成继承链
3. 链路通过rb_task_info->waiting_on字段串联，递归传递
4. mutex释放时，优先级恢复，保证系统调度的实时性和公平性

**相应实现：**

### 6.1 任务信息结构体与链路:

- 通过 struct rb_task_info 结构体中的 original_prio 字段记录任务的原始优先级，waiting_on 字段记录当前正在等待的 mutex


  - 每个任务（task_struct）通过哈希表 rb_task_table 关联到唯一的 rb_task_info，用于优先级继承链的管理

### 6.2 继承链的核心逻辑:

* 当高优先级任务请求一个被低优先级任务持有的 mutex 时，会触发优先级继承

- 继承链通过 `rb_task_info->waiting_on`字段串联，递归传递优先级
- 优先级提升核心函数：

```c
     void propagate_priority(struct task_struct *owner, int new_prio) {
         struct rb_task_info *info = ensure_task_info(owner);
         if (!info) return;

         if (info->original_prio < 0)
             info->original_prio = owner->prio;

         int effective = get_effective_inherited_prio(info);
         if (effective < new_prio)
             effective = new_prio;

         if (effective > 0 && owner->prio > effective) {
             owner->prio = effective;
             pr_info("[rb_mutex] %s inherits prio %d\n", owner->comm, effective);
             wake_up_process(owner);
         }

         // 递归向上传递
         if (info->waiting_on && info->waiting_on->owner) {
             propagate_priority(info->waiting_on->owner, effective);
         }
     }

这里的 info->waiting_on 指向 owner 正在等待的 mutex，如果该 mutex 也有 owner，则继续递归，形成链式优先级继承
```

### 6.3 恢复优先级

* 当 mutex 释放时，调用 restore_priority 恢复任务的原始优先级

- 只有在优先级被提升过（original_prio >= 0）时才会恢复，确保不会误降优先级

###  6.4 继承链的意义

* 递归传递优先级，确保多级锁依赖（如 A 等 B，B 等 C，C 等 D...）时，只要有高优先级任务在最外层等待，所有链上的持有者都会被提升到高优先级，有效避免优先级反转问题，保证实时性和系统公平性



## 7. 时延测试程序的介绍与使用



### 7.1 评估时延测试程序的正确性



## 8. rbmutex的实现过程与细节

### 8.1 结构体设计

####  8.1.1 struct rb_mutex

```c
struct rb_mutex {
    struct task_struct *owner;      // 当前持有该mutex的任务
    struct rb_root waiters;         // 等待该mutex的任务红黑树（按优先级排序）
    spinlock_t lock;                // 保护mutex内部数据结构的自旋锁
    wait_queue_head_t wait_queue;   // 等待队列头，用于阻塞等待的任务
};
```

说明：

- owner：指向当前持有该互斥锁的任务（线程），用于判断锁的归属

- waiters：采用红黑树（rb_tree）结构，管理所有等待该锁的任务，并根据优先级进行排序，保证高优先级任务优先获得锁

- lock：自旋锁，保护互斥锁内部的数据结构，防止并发访问导致的数据不一致

- wait_queue：Linux内核的等待队列，用于实现任务的阻塞与唤醒

####  8.1.2 struct rb_mutex_waiter

```c
struct rb_mutex_waiter {
    struct task_struct *task;   // 等待的任务
    int prio;                   // 该任务的优先级
    struct list_head list;      // 链表节点（备用，便于扩展或调试）
    struct rb_node node;        // 红黑树节点，用于插入到waiters红黑树
};
```

说明：

- task：指向等待该互斥锁的任务

- prio：记录该任务的优先级，便于在红黑树中进行优先级排序

- list：备用链表节点，方便后续扩展或调试

- node：红黑树节点，用于将该等待者插入到rb_mutex的waiters红黑树中

####  8.1.3 struct rb_mutex_waiter

```c
struct rb_task_info {
    struct task_struct *task;       // 关联的任务
    int original_prio;              // 任务的原始优先级（用于优先级继承后恢复）
    struct rb_mutex *waiting_on;    // 当前正在等待的mutex
    struct hlist_node hnode;        // 哈希表节点，用于全局任务信息表
};
```

说明：

- task：指向该任务本身。

- original_prio：记录优先级继承前的原始优先级，便于锁释放时恢复

- waiting_on：指向当前该任务正在等待的mutex，实现优先级继承链

- hnode：用于将rb_task_info插入到全局哈希表，便于快速查找和管理任务信息

#### 8.1.4 设计目标和思路

* ##### 目标:

  结构体的设计使得rbmutex能够高效地管理互斥锁的持有者和等待队列，并实现递归的优先级继承链，保证高优先级任务能够及时获得锁资源，避免优先级反转问题。这为后续的互斥锁操作和优先级继承机制的实现打下了坚实的基础，具体目标如下：

  * 高效管理锁的持有者和等待队列，保证高优先级任务能优先获得锁

  - 支持优先级继承链，递归提升锁链上所有相关任务的优先级，避免优先级反转

  - 便于扩展和维护，为后续功能扩展（如超时、死锁检测等）预留接口

* ##### 思路：

  ###### （1）面向内核调度的高效等待队列

  - 采用红黑树（rb_tree）管理等待队列（waiters），能够在插入、删除、查找最高优先级等待者时保持对数复杂度，适合高并发场景

  - 每个等待者（rb_mutex_waiter）记录自己的优先级，便于排序和调度

  ###### （2）优先级继承链的实现

  - 每个任务通过rb_task_info结构体，记录其原始优先级和当前正在等待的mutex

  - 当高优先级任务请求被低优先级任务持有的锁时，递归提升锁链上所有持有者的优先级（优先级继承链），保证高优先级任务不会被低优先级任务“拖慢”

  ###### （3）并发安全与调度友好

  - 通过自旋锁（spinlock）保护互斥锁内部状态，确保多核/多线程环境下的数据一致性

  - 结合内核等待队列（wait_queue_head_t），实现任务的阻塞与唤醒，配合优先级继承机制，提升系统实时性

  ###### （4）便于扩展和维护

  - 结构体中预留了链表节点、哈希表节点等，方便后续功能扩展（如统计、调试、死锁检测等）

  - 采用分层设计，结构体之间通过指针关联，便于管理和维护

### 8.2 基本操作流程（锁的初始化、加锁、解锁等）

#### 8.2.1 rb_mutex_init

```c
void rb_mutex_init(struct rb_mutex *mutex)
{
    mutex->owner = NULL;
    mutex->waiters = RB_ROOT;
    spin_lock_init(&mutex->lock);
    init_waitqueue_head(&mutex->wait_queue);
}
```

- 初始化一个 rbmutex 实例，设置 owner 为 NULL，初始化等待队列（红黑树）、自旋锁和内核等待队列

####  8.2.2 rb_mutex_lock

```c
void rb_mutex_lock(struct rb_mutex *mutex)
{
    DEFINE_WAIT(wait);
    struct rb_mutex_waiter self;
    struct rb_task_info *info = ensure_task_info(current);

    self.task = current;
    self.prio = current->prio;
    INIT_LIST_HEAD(&self.list);

    spin_lock(&mutex->lock);
    if (!mutex->owner) {
        mutex->owner = current;
        if (info) info->waiting_on = NULL;
        spin_unlock(&mutex->lock);
        return;
    }

    rb_mutex_enqueue(mutex, &self);
    if (info) info->waiting_on = mutex;
    propagate_priority(mutex->owner, current->prio);
    spin_unlock(&mutex->lock);

    for (;;) {
        prepare_to_wait(&mutex->wait_queue, &wait, TASK_UNINTERRUPTIBLE);
        spin_lock(&mutex->lock);
        if (mutex->owner == NULL && rb_mutex_top_waiter(mutex) == &self) {
            rb_mutex_dequeue(mutex, &self);
            mutex->owner = current;
            if (info) info->waiting_on = NULL;
            spin_unlock(&mutex->lock);
            break;
        }
        spin_unlock(&mutex->lock);
        schedule();
    }
    finish_wait(&mutex->wait_queue, &wait);
}
```

- 获取互斥锁。如果锁空闲，当前任务直接获得锁

- 如果锁已被占用，将当前任务按优先级插入等待队列，并阻塞等待

- 在等待过程中，自动触发优先级继承机制，递归提升锁链上相关任务的优先级

####  8.2.3 rb_mutex_unlock

```c
void rb_mutex_unlock(struct rb_mutex *mutex)
{
    spin_lock(&mutex->lock);
    if (mutex->owner != current) {
        spin_unlock(&mutex->lock);
        return;
    }
    mutex->owner = NULL;
    restore_priority(current);

    if (!RB_EMPTY_ROOT(&mutex->waiters)) {
        struct rb_mutex_waiter *next = rb_mutex_top_waiter(mutex);
        wake_up_process(next->task);
    }
    spin_unlock(&mutex->lock);
}
```

- 释放互斥锁

- 如果有等待队列，唤醒优先级最高的等待者

- 锁释放时自动恢复当前任务的原始优先级（如有继承）

#### 8.2.4 rb_mutex_trylock

```c
int rb_mutex_trylock(struct rb_mutex *mutex)
{
    int success = 0;
    spin_lock(&mutex->lock);
    if (!mutex->owner) {
        mutex->owner = current;
        struct rb_task_info *info = ensure_task_info(current);
        if (info) info->waiting_on = NULL;
        success = 1;
    }
    spin_unlock(&mutex->lock);
    return success;
}
```

- 尝试获取互斥锁

- 如果锁空闲则立即获得锁并返回1，否则返回0，不阻塞当前任务

#### 8.2.5 rb_mutex_lock_timeout

```c
int rb_mutex_lock_timeout(struct rb_mutex *mutex, unsigned long timeout_ms)
{
    DEFINE_WAIT(wait);
    struct rb_mutex_waiter self;
    struct rb_task_info *info = ensure_task_info(current);
    unsigned long deadline = jiffies + msecs_to_jiffies(timeout_ms);

    if (mutex->owner == current) {
        pr_warn("[rb_mutex] Deadlock detected: %s tried to re-lock\n", current->comm);
        return -EDEADLK;
    }

    self.task = current;
    self.prio = current->prio;
    INIT_LIST_HEAD(&self.list);

    spin_lock(&mutex->lock);
    if (!mutex->owner) {
        mutex->owner = current;
        if (info) info->waiting_on = NULL;
        spin_unlock(&mutex->lock);
        return 0;
    }

    rb_mutex_enqueue(mutex, &self);
    if (info) info->waiting_on = mutex;
    propagate_priority(mutex->owner, current->prio);
    spin_unlock(&mutex->lock);

    for (;;) {
        prepare_to_wait(&mutex->wait_queue, &wait, TASK_UNINTERRUPTIBLE);

        spin_lock(&mutex->lock);
        if (mutex->owner == NULL && rb_mutex_top_waiter(mutex) == &self) {
            rb_mutex_dequeue(mutex, &self);
            mutex->owner = current;
            if (info) info->waiting_on = NULL;
            spin_unlock(&mutex->lock);
            break;
        }
        spin_unlock(&mutex->lock);

        if (time_after(jiffies, deadline)) {
            rb_mutex_dequeue(mutex, &self);
            if (info) info->waiting_on = NULL;
            finish_wait(&mutex->wait_queue, &wait);
            return -ETIMEDOUT;
        }

        schedule_timeout(msecs_to_jiffies(10));
    }

    finish_wait(&mutex->wait_queue, &wait);
    return 0;
}
```

- 在指定超时时间内尝试获取互斥锁，超时未获得锁则从等待队列移除并返回超时错误

​	防止任务因锁竞争而无限期阻塞，提升系统健壮性

#### 8.2.6设计目标与思路

* ##### 目标

  * 高效管理锁的竞争与释放：通过合理的数据结构和调度机制，确保互斥锁的加锁、解锁操作在高并发场景下依然高效，减少系统开销
  * 支持优先级继承：自动递归提升锁链上相关任务的优先级，避免优先级反转，保证高优先级任务的实时性
  * 灵活的接口设计：提供阻塞、非阻塞、超时等多种加锁方式，满足不同应用场景的需求
  * 并发安全与可扩展性：采用自旋锁和等待队列机制，保证多核/多线程环境下的安全性，并为后续功能扩展（如死锁检测）预留空间

* ##### 思路

  1. ###### 初始化流程

     - 每个互斥锁在使用前都通过rb_mutex_init进行初始化，确保所有成员变量处于安全、可用的初始状态

  2. ###### 加锁流程

     - 如果锁空闲，当前任务直接获得锁

     - 如果锁已被占用，当前任务根据优先级插入等待队列，并阻塞等待

     - 在等待过程中，自动触发优先级继承机制，递归提升锁链上相关任务的优先级

  3. ###### 解锁流程

     - 释放锁后，自动恢复当前任务的原始优先级

     - 唤醒等待队列中优先级最高的任务，保证高优先级任务优先获得锁

  4. ###### 非阻塞与超时加锁

     - rb_mutex_trylock提供非阻塞加锁方式，适合对实时性要求极高的场景

     - rb_mutex_lock_timeout支持超时机制，防止任务因锁竞争而无限期阻塞

  5. ###### 并发保护

     所有关键操作均通过自旋锁保护，确保多核环境下的数据一致性和操作原子性

### 8.3 等待队列与优先级排序

#### 8.3.1 rb_mutex_enqueue

```c
static void rb_mutex_enqueue(struct rb_mutex *mutex, struct rb_mutex_waiter *waiter)
{
    struct rb_node **new = &mutex->waiters.rb_node, *parent = NULL;
    struct rb_mutex_waiter *entry;

    while (*new) {
        parent = *new;
        entry = rb_entry(parent, struct rb_mutex_waiter, node);
        if (waiter->prio > entry->prio)
            new = &(*new)->rb_left;
        else
            new = &(*new)->rb_right;
    }

    rb_link_node(&waiter->node, parent, new);
    rb_insert_color(&waiter->node, &mutex->waiters);
}
```

- 从红黑树根节点开始，按照优先级（waiter->prio）递归查找插入位置

- 优先级高的节点插入到左子树，优先级低的插入到右子树，保证红黑树有序

- 插入后调用rb_insert_color维持红黑树的平衡性

#### 8.3.2 rb_mutex_dequeue

```c
static void rb_mutex_dequeue(struct rb_mutex *mutex, struct rb_mutex_waiter *waiter)
{
    rb_erase(&waiter->node, &mutex->waiters);
    waiter->node.rb_left = waiter->node.rb_right = NULL;
}
```

- 调用rb_erase将节点从红黑树中删除，并清空其左右子节点指针

#### 8.3.3 rb_mutex_top_waiter

```c
static struct rb_mutex_waiter *rb_mutex_top_waiter(struct rb_mutex *mutex)
{
    struct rb_node *node = mutex->waiters.rb_node;
    if (!node)
        return NULL;
    while (node->rb_left)
        node = node->rb_left;
    return rb_entry(node, struct rb_mutex_waiter, node);
}
```

- 从红黑树根节点出发，始终向左子树遍历，直到最左端节点，即为优先级最高的等待者

#### 8.3.4 设计目标和思路

* ##### 目标

  * 保证高优先级任务优先获得锁：通过合理的队列管理机制，确保在多个任务竞争同一个互斥锁时，优先级最高的任务能够最先获得锁，避免“饿死”现象
  * 高效的队列操作性能：在插入、删除、查找等待任务时，保持较低的时间复杂度，适应高并发和实时性要求
  * 为优先级继承机制提供基础：通过有序的等待队列，便于在优先级继承时快速定位和提升相关任务的优先级

* ##### 思路

  1. ###### 红黑树管理等待队列

     - 每个rb_mutex结构体中包含一个rb_root waiters成员，采用红黑树结构管理所有等待该锁的任务

     - 红黑树是一种自平衡二叉查找树，能够在O(logN)时间内完成插入、删除和查找操作，适合高并发场景

  2. ###### 等待者结构体记录优先级

     - 每个等待任务通过rb_mutex_waiter结构体表示，其中包含prio字段，记录该任务的优先级

     - 在插入红黑树时，按照prio字段进行排序，保证树的左子树始终为更高优先级的等待者

  3. ###### 插入与删除操作

     - 加锁时，如果锁已被占用，当前任务会被封装为rb_mutex_waiter并插入到红黑树中，插入过程根据优先级排序

     - 解锁或超时等场景下，从红黑树中删除对应的等待者节点

  4. ###### 查找最高优先级等待者

     - 通过遍历红黑树的最左节点，能够快速定位优先级最高的等待者，实现高效唤醒

  5. ###### 与优先级继承机制协同

     - 有序的等待队列为优先级继承提供了基础，便于在加锁、解锁等操作中递归提升锁链上相关任务的优先级



* ### 优先级继承链实现

  * #### propagate_priority

    ```c
    static void propagate_priority(struct task_struct *owner, int prio)
    {
        struct rb_task_info *info = ensure_task_info(owner);
        if (!info) return;
    
        if (owner->prio > prio) {
            if (info->original_prio < 0)
                info->original_prio = owner->prio;
    
            owner->prio = prio;
            pr_info("[rb_mutex] %s inherits prio %d\n", owner->comm, prio);
    
            set_tsk_need_resched(owner);
            wake_up_process(owner);
    
            if (info->waiting_on && info->waiting_on->owner)
                propagate_priority(info->waiting_on->owner, prio);
        }
    }
    ```

    - 只有当当前持有者优先级高于传入优先级时才提升


    - 记录原始优先级并提升当前持有者优先级
    
    - 如果持有者也在等待其他锁，则递归调用自身，继续向上提升优先级

  * #### restore_priority

    ```c
    static void restore_priority(struct task_struct *task)
    {
        struct rb_task_info *info = get_task_info(task);
        if (!info) return;
    
        if (info->original_prio >= 0 && task->prio != info->original_prio) {
            pr_info("[rb_mutex] restore prio for %s to %d\n", task->comm, info->original_prio);
            task->prio = info->original_prio;
            info->original_prio = -1;
    
            set_tsk_need_resched(task);
            wake_up_process(task);
        }
    }
    ```

    - 检查original_prio是否有效，如果有效则恢复，并提示调度器重新调度

  * #### 设计目标与思路

    * ##### 目标：

      * 彻底解决优先级反转问题：保证高优先级任务在锁竞争链路上不会被低优先级任务阻塞，提升系统实时性
      * 支持多级锁依赖的递归继承：当锁依赖形成链路（如A等B，B等C），高优先级任务的优先级能递归传递到所有相关持有者
      * 自动恢复原始优先级：当锁释放后，能自动恢复任务的原始优先级，防止优先级“污染”影响后续调度

    * ##### 思路：

      1. ###### 任务信息结构体链路

         - 每个任务通过rb_task_info结构体，记录其原始优先级（original_prio）和当前正在等待的mutex（waiting_on）

         - 通过waiting_on字段，将任务与其等待的锁串联起来，形成优先级继承链

      2. ###### 递归优先级提升

         - 当高优先级任务请求被低优先级任务持有的锁时，调用propagate_priority函数

         - 该函数会递归地将高优先级传递给锁的持有者，如果持有者本身也在等待另一个锁，则继续向上递归，形成链式优先级继承

      3. ###### 优先级恢复

         - 当任务释放锁时，调用restore_priority函数，自动恢复其原始优先级

      4. ###### 调度器交互

         * 在优先级发生变化时，主动调用set_tsk_need_resched和wake_up_process，提示调度器及时调度高优先级任务

* ### 调度器交互优化

  * #### 接口

    * ##### set_tsk_need_resched

    * ##### wake_up_process

    * ##### 结合优先级继承链

      在优先级变化的关键函数中，均调用上述接口

      ```c
      owner->prio = prio;
      set_tsk_need_resched(owner);
      wake_up_process(owner);
      ```

      ```c
      task->prio = info->original_prio;
      set_tsk_need_resched(task);
      wake_up_process(task);
      ```

  * #### 设计目标与思路

    * ##### 目标:

      * 确保优先级变化能被调度器及时感知：当任务的优先级发生提升或恢复时，调度器能够第一时间重新评估调度顺序，避免高优先级任务被低优先级任务延迟
      * 最大化高优先级任务的响应速度：通过主动与调度器交互，减少高优先级任务等待CPU的时间，提升系统实时性

    * ##### 思路：

      1. ###### 主动提示调度器重新调度

         - 在优先级发生变化（提升或恢复）时，调用set_tsk_need_resched，标记目标任务需要重新调度

         - 这样调度器会在下次调度点优先考虑该任务

      2. ###### 唤醒休眠中的高优先级任务

         * 调用wake_up_process，如果目标任务处于休眠状态，则立即唤醒，使其有机会被调度运行

      3. ###### 与优先级继承链结合

         * 在优先级递归提升或恢复的每一步，都及时与调度器交互，确保链路上的每个相关任务都能尽快获得调度机会

* ### 其他细节与扩展

  * #### 在优先级继承和恢复相关函数中，主动与调度器交互

    ```c
    // 在 rb_mutex.c->propagate_priority 中
    set_tsk_need_resched(owner);   // 标记需要调度
    wake_up_process(owner);        // 如果在休眠，则唤醒立即重新调度
    
    // 在 rb_mutex.c->restore_priority 中
    set_tsk_need_resched(task);
    wake_up_process(task);
    ```
    
    * 在优先级提升时，调用 set_tsk_need_resched(owner) 和 wake_up_process(owner)，确保调度器能及时感知优先级变化，并立即唤醒高优先级任务

    - 在优先级恢复时，同样调用上述接口，保证任务优先级恢复后能尽快重新参与调度

    - 这样可以让高优先级任务更快获得 CPU，显著提升系统的实时性和响应速度，避免高优先级任务因调度器“滞后”而被低优先级任务延迟

  * #### 死锁
  
    * ##### 死锁检测机制
  
      * 在加锁操作（如 rb_mutex_lock 或 rb_mutex_lock_timeout）中，系统会检测当前任务请求的锁是否会导致锁依赖环路
      * 具体做法是：沿着 rb_task_info->waiting_on 链递归遍历，检查是否存在环路（即某个任务最终又等待了自己持有的锁）
  
    * ##### 实现细节
  
      - 检测到死锁时，系统会打印详细的锁依赖链路日志和死锁计数统计，便于开发者定位和分析死锁原因
  
      - 死锁检测函数通常返回特定错误码（如 -EDEADLK），调用者可据此采取相应措施

## 9. rbrwmutex的实现过程与细节

### 9.1 结构体设计

```c
struct rb_rwmutex {
    struct rb_mutex lock;      // 保护内部状态的互斥锁
    struct rb_mutex wr_lock;   // 写锁（独占）
    int readers;               // 当前持有读锁的线程数
};
```

- lock：用于保护readers计数和写锁的状态，防止并发修改。

- wr_lock：写锁，保证写操作的独占性。

- readers：当前持有读锁的线程数。

### 9.2 核心操作流程

#### 9.2.1 初始化

```c
void rb_rwmutex_init(struct rb_rwmutex *rw) {
    rb_mutex_init(&rw->lock);
    rb_mutex_init(&rw->wr_lock);
    rw->readers = 0;
}
```

- 初始化内部互斥锁和写锁，读者计数清零

#### 9.2.2 读锁加锁

```c
void rb_rwmutex_read_lock(struct rb_rwmutex *rw) {
    rb_mutex_lock(&rw->lock);
    if (rw->readers == 0) {
        // 第一个读者需要获得写锁，防止写者进入
        rb_mutex_lock(&rw->wr_lock);
    }
    rw->readers++;
    rb_mutex_unlock(&rw->lock);
}
```

- 第一个读者加锁时，先获得写锁，防止写者进入

- 读者计数加一，释放保护锁

#### 9.2.3 读锁解锁

```c
void rb_rwmutex_read_unlock(struct rb_rwmutex *rw) {
    rb_mutex_lock(&rw->lock);
    rw->readers--;
    if (rw->readers == 0) {
        // 最后一个读者释放写锁
        rb_mutex_unlock(&rw->wr_lock);
    }
    rb_mutex_unlock(&rw->lock);
}
```

* 读者计数减一，最后一个读者释放写锁

* #### 写锁加锁/解锁

  ```c
  void rb_rwmutex_write_lock(struct rb_rwmutex *rw) {
      rb_mutex_lock(&rw->wr_lock);
  }
  
  void rb_rwmutex_write_unlock(struct rb_rwmutex *rw) {
      rb_mutex_unlock(&rw->wr_lock);
  }
  ```

  * 写者直接竞争写锁，保证写操作的独占性

### 9.3 设计目标与思路

* #### 目标：

  * 支持多读单写，保证写操作的独占性和读操作的并发性

  - 继承rb_mutex的优先级继承和死锁检测机制，提升实时性和健壮性

  - 结构简单，易于维护和扩展

* #### 思路：

  1. 采用“第一个读者加写锁，最后一个读者释放写锁”的经典读写锁设计，防止写者饥饿
  2. 所有锁操作均基于rb_mutex，天然支持优先级继承链和死锁检测
  3. 通过内部lock保护读者计数，保证多核并发安全

## 10. rbrwsem的实现过程与细节

### 10.1 结构体设计

```c
struct rb_rwsem {
    struct rb_mutex lock;      // 保护内部状态
    struct rb_mutex wr_lock;   // 写锁（独占）
    int readers;               // 当前持有读锁的线程数
};
```

- lock：用于保护readers计数和写锁状态，防止并发修改

- wr_lock：写锁，保证写操作的独占性

- readers：当前持有读锁的线程数

### 10.2 核心操作流程

#### 10.2.1 初始化

```c
void rb_rwsem_init(struct rb_rwsem *sem) {
    rb_mutex_init(&sem->lock);
    rb_mutex_init(&sem->wr_lock);
    sem->readers = 0;
}
```

* 初始化内部互斥锁和写锁，读者计数清零

#### 10.2.2 读锁加锁

```c
void rb_rwsem_down_read(struct rb_rwsem *sem) {
    rb_mutex_lock(&sem->lock);
    if (sem->readers == 0) {
        rb_mutex_lock(&sem->wr_lock);
    }
    sem->readers++;
    rb_mutex_unlock(&sem->lock);
}
```

- 第一个读者加锁时，先获得写锁，防止写者进入

- 读者计数加一，释放保护锁

#### 10.2.3 读锁解锁

```c
void rb_rwsem_up_read(struct rb_rwsem *sem) {
    rb_mutex_lock(&sem->lock);
    sem->readers--;
    if (sem->readers == 0) {
        rb_mutex_unlock(&sem->wr_lock);
    }
    rb_mutex_unlock(&sem->lock);
}
```

* 读者计数减一，最后一个读者释放写锁

#### 10.2.4 写锁加锁/解锁

```c
void rb_rwsem_down_write(struct rb_rwsem *sem) {
    rb_mutex_lock(&sem->wr_lock);
}

void rb_rwsem_up_write(struct rb_rwsem *sem) {
    rb_mutex_unlock(&sem->wr_lock);
}
```

* 写者直接竞争写锁，保证写操作的独占性

### 10.3 设计目标与思路

* #### 目标:

  * 实现多读单写的信号量机制，支持优先级继承和死锁检测

  - 适用于高并发、实时性要求高的内核场景

  - 结构简单，易于维护和扩展

* #### 思路：

  1. 采用“第一个读者加写锁，最后一个读者释放写锁”的经典读写信号量设计，防止写者饥饿
  2. 所有锁操作均基于 rb_mutex，天然支持优先级继承链和死锁检测
  3. 通过内部 lock 保护读者计数，保证多核并发安全



## 11. 内核宏开关的实现过程与细节



## 12. 创新点

### 12.1 死锁机制

#### 12.1.1 递归链路检测与可视化链路日志

- 死锁检测不仅仅是简单判断当前锁是否被自己持有，而是递归遍历 owner->waiting_on 链，检测是否存在环路（即死锁）

- 检测过程中，详细记录锁依赖链路，包括任务名、PID、锁地址等，便于后续分析

- 创新性地生成 Mermaid 流程图格式的链路日志，可直接用于可视化工具，极大提升了死锁调试的直观性和效率

#### 12.1.2 高效的数据结构与低开销检测

- 利用每个任务的 rb_task_info->waiting_on 字段，链式串联锁依赖关系，递归检测环路，无需全局扫描所有锁和任务，检测效率高

- 检测深度有限制（如最大64层），防止异常情况下死锁检测本身导致系统卡死

#### 12.1.3 死锁统计与接口导出

- 死锁发生时，原子计数器统计死锁次数，可通过接口 rb_mutex_get_deadlock_count() 查询，便于系统监控和自动化测试

- 死锁检测返回专用错误码（如 RB_MUTEX_DEADLOCK_ERR），调用者可据此做出特殊处理（如报警、拒绝加锁等）

#### 12.1.4 与优先级继承链天然结合

- 死锁检测逻辑与优先级继承链递归遍历共用同一套链路结构，既能递归提升优先级，也能递归检测死锁，实现机制复用，简化代码和维护

#### 12.1.5 详细日志与开发者友好

- 死锁检测失败时，打印详细的链路日志，包括任务、锁依赖、链路路径，极大方便开发者定位和复现问题

- 支持递归锁检测和死锁链路的详细输出，便于复杂场景下的调试

  

### 12.2 时延测试程序

* ### 内核宏开关设计

  

## 13. 效果呈现——全过程演示

- 此模块内容附上百度网盘链接以展示基本功能实现效果。

- [！视频文件点这里直达 ！](https://pan.baidu.com/s/1u-9w99XCO6ZT2nRdnjsZnA?pwd=jt1f)

- 百度网盘提取码: jt1f

  

## 14. 开发中遇到的问题与解决方法

### 14.1 **优先级继承链递归传递的正确性**

- 问题：初期实现时，优先级继承只提升了直接持有锁的任务，未能递归传递到锁链上所有相关任务，导致多级锁依赖下仍可能发生优先级反转


   - 解决方法：引入 rb_task_info->waiting_on 字段，递归调用 propagate_priority，确保高优先级任务的优先级能沿锁依赖链传递到所有持有者，彻底解决优先级反转

### 14.2 **死锁检测的高效性与可用性**

- 问题：死锁检测如果全局遍历所有锁和任务，开销大且实现复杂，且难以定位具体死锁链路


   - 解决方法：采用链式递归检测（rb_mutex_detect_deadlock），只需沿当前加锁请求的 owner->waiting_on 链递归遍历，效率高并创新性地输出详细链路日志和 Mermaid 可视化格式，极大提升调试效率

### 14.3 **递归锁与重复加锁检测**

- 问题：任务重复加锁同一互斥锁时，容易导致死锁或逻辑错误


   - 解决方法：在加锁入口检测 mutex->owner == current，发现递归加锁立即返回错误并输出警告日志，防止死锁

### 14.4 **内核模块符号导出与依赖管理**

- 问题：多个模块间调用时，符号未正确导出或Makefile依赖未处理，导致编译或加载失败


   - 解决方法：通过 EXPORT_SYMBOL 导出所有核心接口，完善 Makefile 依赖关系，确保各模块可独立编译和正确链接

### 14.5 **高并发下的性能与数据一致性**

- 问题：高并发场景下，锁内部状态易被竞争访问，导致数据不一致或死锁检测失效


   - 解决方法：所有关键路径均加自旋锁保护，保证多核/多线程环境下的数据一致性和操作原子性

### 14.6 **测试用例覆盖不全**

- 问题：初期测试用例未覆盖优先级继承链、死锁、递归锁等极端场景，导致部分bug难以暴露


   - 解决方法：补充多种复杂场景的测试用例，包括多级锁依赖、死锁链路、优先级动态变化等，确保功能健壮性

### 14.7 **日志与调试信息不足**

- 问题：早期日志信息有限，难以定位复杂问题


   - 解决方法：增加详细日志输出，包括锁操作、优先级变化、死锁链路等，便于开发和维护

### 14.8 **优先级恢复时机与调度器交互**

- 问题：优先级恢复后，任务未能及时被调度，影响高优先级任务响应


   - 解决方法：在优先级提升和恢复时，主动调用 set_tsk_need_resched 和 wake_up_process，提示调度器及时调度高优先级任务

     


## 15. 分工和协作

| 成员身份   | 分工                                                         |
| ---------- | ------------------------------------------------------------ |
| 队长钟家意 | 事务协调，比赛资源检索与分配，代码编写，实验文档撰写         |
| 队员汪文琦 | 任务分析，比赛资源检索与分配，协调任务，代码编写，实验文档撰写 |
| 队员慕欣杭 | 任务分析，代码编写，日志撰写，协作库管理                     |

## 16. 情况说明

> 补丁发送到社区主线(可选)

因为我们的项目不够精美完善，还有待继续开发进行代码优化，所以暂未上传社区

## 17. 比赛心得与收获

### 17.1 心得体会

1. **团队协作的重要性**: 在整个项目开发过程中，我们深刻体会到了团队协作的力量。每个成员都发挥了自己的特长，相互配合，共同克服了许多困难。团队内部的沟通和协作，使得我们能够在有限的时间内高效地完成各项任务。
2. **系统化的项目管理**: 本次比赛对我们来说是一次非常宝贵的项目管理实践。从项目目标的设定、任务的分解，到进度的跟踪、风险的预判，我们逐步掌握了一套系统化的项目管理方法。这不仅提高了我们的工作效率，也增强了我们对项目整体把控的能力。
3. **技术积累与提升**: 通过本次项目开发，我们在Linux内核同步机制、并发控制、内核模块开发等方面的技术能力得到了显著提升。特别是在优先级继承链设计、死锁检测机制实现、内核数据结构（如红黑树、哈希表）应用、内核日志与调试、以及多线程/多核环境下的性能优化等方面，我们积累了丰富的实战经验。这些技术积累不仅提升了我们解决复杂系统级问题的能力，也为今后从事操作系统、内核开发等高端技术领域打下了坚实基础。
4. **面对挑战的勇气和韧性**: 在本次内核锁机制项目开发过程中，我们遇到了诸多技术难题和实现瓶颈。例如，如何实现递归的优先级继承链，如何高效检测和可视化死锁，如何保证多核环境下的数据一致性与高性能，以及如何设计健壮的测试用例覆盖极端并发场景。每一次遇到的挑战都促使我们不断查阅资料、分析源码、反复调试和优化方案。通过团队协作和坚持不懈的努力，我们不仅攻克了这些技术难关，也锻炼了面对复杂问题时的勇气和韧性，提升了独立思考和解决问题的能力。

### 17.2 比赛收获

1. **深刻理解了Linux内核锁机制的开发**: 在项目开发过程中，我们系统性地学习并实践了Linux内核中互斥锁、读写锁、信号量等多种同步原语的实现原理。通过查阅内核源码、分析调度与同步机制，我们不仅掌握了锁的基本用法，还深入理解了优先级继承、死锁检测、锁等待队列等高级特性。特别是在实现自定义rb_mutex时，我们亲自设计了数据结构、实现了递归优先级继承链和高效的死锁检测机制，全面体验了内核级锁开发的复杂性与严谨性。这一过程极大提升了我们对操作系统底层并发控制和实时性的理解，为后续深入内核开发打下了坚实基础。
2. **提升了实际动手能力**: 比赛过程中，我们不仅学习了很多理论知识，更重要的是将这些知识应用到了实际项目中。通过亲手实践，我们对这些技术有了更深刻的理解和体会。
3. **拓宽了技术视野**: 比赛中，我们接触到了许多前沿技术和工具，例如OpenEuler操作系统与4.19.325内核，Vscode远程连接服务器等。这些新知识和新技能拓宽了我们的技术视野，为我们今后的职业发展打下了坚实的基础。
4. **积累了宝贵的项目经验**: 通过这次比赛，我们积累了丰富的项目经验，从中学到了很多宝贵的教训和心得。这些经验不仅有助于我们在未来的项目中更好地应对各种挑战，也增强了我们的职业自信心。

------

## 18. 参考文献

1. https://gitee.com/EastNoMoney/linux-5.6/blob/master/Documentation/locking/rt-mutex-design.rst
2. [Utilization inversion and proxy execution](https://lwn.net/Articles/820575)
3. [CONFIG_RT_MUTEX_FULL](https://rt.wiki.kernel.org/index.php/Main_Page)
4. [Linux Futex PI](https://www.kernel.org/doc/Documentation/pi-futex.txt)



​	
