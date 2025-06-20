# proj62-linux-lock-priority-inheritance

### 项目名称

Linux 锁优先级传递

### 项目描述

优先级翻转是指当一个高优先级任务通过锁等机制访问共享资源时, 该锁如果已被一低优先级任务占有, 将可能造成高优先级任务被许多具有较低优先级任务阻塞.
优先级翻转问题是影响 linux 实时性的障碍之一, 甚至引起很多稳定性问题.

### 所属赛道

2025全国大学生操作系统比赛的"OS功能设计"赛道

### 项目导师

谢秀奇、[成坚](https://kernel.blog.csdn.net)

* gitee/github [Xie XiuQi](https://gitee.com/xiexiuqi), [Cheng Jian](https://github.com/gatieme)

* email xiexiuqi@huawei.com, cj.chengjian@huawei.com

### 特征

*	优先级继承、传递

*	实时性

### 文档

[Utilization inversion and proxy execution](https://lwn.net/Articles/820575)

[CONFIG_RT_MUTEX_FULL](https://rt.wiki.kernel.org/index.php/Main_Page)

[Linux Futex PI](https://www.kernel.org/doc/Documentation/pi-futex.txt)


### License

* [GPL-2.0](https://opensource.org/licenses/GPL-2.0)

### 该项目的目标:

1.	实现 rwlock、rwsem、mutex 的优先级继承支持.

2.	通过内核宏开关优先级继承相关特性

## 预期目标

* 描述算法思路, 实现相关算法, 可以编译安装运行

* 提供与原生锁等在时延方面的测试数据

* 补丁发送到社区主线(可选)

## 预计提交什么
1. 完整的设计方案文档：设计思路、实现描述、代码说明、研发过程中遇到的问题和解决方法、在本队作品源码和文档中存在的非本队来源说明等。
2. 项目源代码
3. 项目测试结果的功能/性能/创新性等的分析（包括与类似项目的对比分析）。
4. 作品进展汇报幻灯片：项目参赛队员分工，项目开发时间进度安排及相应任务的推进与完成情况。
5. 演示视频：作品演示视频内容包括但不限于：项目源代码运行环境介绍、符合项目任务要求的运行演示效果及结果说明。

## 项目计划
1. 复现去年队伍的成果
2. 实现mutex的优先级继承
3. 实现rwlock的优先级继承
4. 实现rwsem的优先级继承
5. 实现内核宏开关优先级继承的特性
6. 测试，提供3个锁与原生锁在时延方面的测试数据
7. 完成文档，提交作品给大赛

## 瓶颈
- 怎么运行4.19.325内核以及修改后的内核
    - 解决办法：统一用安装openeuler的虚拟机，通过ssh将虚拟机连接到vscode上便于开发
  
## 任务
  1. rtmutex.h .c.h全看
  2. rtmutex.c分模块读，然后简化。三个人分工，问AI。
