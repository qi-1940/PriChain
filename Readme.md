# proj62-linux-lock-priority-inheritance

### 项目名称

Linux 锁优先级传递

### 项目描述

优先级翻转是指当一个高优先级任务通过锁等机制访问共享资源时, 该锁如果已被一低优先级任务占有, 将可能造成高优先级任务被许多具有较低优先级任务阻塞.
优先级翻转问题是影响 linux 实时性的障碍之一, 甚至引起很多稳定性问题.


该项目的目标:

1.	实现 rwlock、rwsem、mutex 的优先级继承支持.

2.	通过内核宏开关优先级继承相关特性


### 所属赛道

2025全国大学生操作系统比赛的"OS功能设计"赛道




### 参赛要求

- 以小组为单位参赛，最多三人一个小组，且小组成员是来自同一所高校的本科生
- 如学生参加了多个项目, 参赛学生选择一个自己参加的项目参与评奖
- 请遵循"2025全国大学生操作系统比赛"的章程和技术方案要求



### 项目导师

谢秀奇、[成坚](https://kernel.blog.csdn.net)

* gitee/github [Xie XiuQi](https://gitee.com/xiexiuqi), [Cheng Jian](https://github.com/gatieme)

* email xiexiuqi@huawei.com, cj.chengjian@huawei.com


### 难度


中等



### 特征


*	优先级继承、传递

*	实时性

### 文档

[Utilization inversion and proxy execution](https://lwn.net/Articles/820575)

[CONFIG_RT_MUTEX_FULL](https://rt.wiki.kernel.org/index.php/Main_Page)

[Linux Futex PI](https://www.kernel.org/doc/Documentation/pi-futex.txt)


### License

* [GPL-2.0](https://opensource.org/licenses/GPL-2.0)



## 预期目标

### 注意：下面的内容是建议内容，不要求必须全部完成。选择本项目的同学也可与导师联系，提出自己的新想法，如导师认可，可加入预期目标

### 基于 Linux kernel 4.19 或 5.10 或社区主线实现

* 描述算法思路, 实现相关算法, 可以编译安装运行

* 提供与原生锁等在时延方面的测试数据

* 补丁发送到社区主线(可选)
