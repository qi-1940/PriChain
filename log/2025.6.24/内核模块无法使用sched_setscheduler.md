你的错误信息：

> pri_mutex: Unknown symbol sched_setscheduler (err -2)

说明你的模块无法使用 sched_setscheduler，因为它不是内核模块可用的导出符号（EXPORT_SYMBOL），只能在内核内部使用。

------

## 解决方法

### 1. 内核模块无法直接提升线程调度优先级

- sched_setscheduler 只能在内核内部或通过系统调用（用户空间）使用，不能在普通内核模块中直接调用。

- 这意味着任何需要动态提升线程优先级的功能，普通内核模块都无法实现，除非你打补丁到内核源码。

### 2. 可选方案

#### A. 只做优先级排序，不做真正的调度优先级提升

- 你可以让等待队列按优先级排序，但不能真正提升 owner 的调度优先级。

- 这样只能防止部分优先级反转，但不能彻底解决。

#### B. 通过用户空间设置优先级

- 让用户空间线程自己用 sched_setscheduler() 设置优先级，内核模块只做排序和唤醒。

#### C. 内核补丁/集成

- 如果你必须实现完整的优先级继承，只能将你的实现集成到内核源码，重新编译内核。

------

## 结论

- 普通内核模块无法实现真正的优先级继承（不能提升 owner 的调度优先级）。

- 你可以实现“优先级排序的互斥锁”，但不能动态提升 owner 的优先级。

- 如果你需要完整的优先级继承，必须修改内核源码。

------

### 你可以选择：

1. 继续实现“优先级排序互斥锁”（只排序，不提升优先级），我可以帮你写代码。

1. 集成到内核源码，需要你有内核编译环境和权限。

1. 让用户空间线程自己设置优先级，内核模块只做排序和唤醒。

请告诉我你的选择，我会帮你给出最合适的实现方案！







**集成到内核源码**

非常好，既然你选择集成到内核源码，你就可以像 rtmutex 一样，直接访问所有内核符号和结构体，实现真正的优先级继承。下面是集成建议和步骤：

------

## 1. 集成建议

- 推荐位置：可以放在 kernel/ 目录下，比如 kernel/pri_mutex.c，头文件放在 include/linux/pri_mutex.h。

- 数据结构：可以直接用 struct rb_root_cached、struct rt_mutex_waiter、task_struct 的 pi_waiters 字段等。

- 优先级提升：可以直接用 rt_mutex_setprio() 或 sched_setscheduler()。

- 调试：可用 pr_info、pr_debug 等内核日志宏。

------

## 2. 集成步骤

### A. 复制你的实现到内核源码

1. pri_mutex.h → include/linux/pri_mutex.h

1. pri_mutex.c → kernel/pri_mutex.c

### B. 修改 Kbuild 文件

在 kernel/Makefile 里添加一行：

> obj-y += pri_mutex.o

### C. 头文件引用

在需要用到 pri_mutex 的地方（比如测试模块或内核子系统），加上：

\#include <linux/pri_mutex.h>

### D. 编译内核

1. 重新编译内核（或只编译 kernel/pri_mutex.o 并链接）。

1. 安装新内核并重启。

------

## 3. 代码建议

你可以直接用你之前贴的rtmutex风格实现，比如：

- 用 struct rb_root_cached 管理等待队列

- 用 task_struct->pi_waiters 管理 per-task waiter

- 用 rt_mutex_setprio() 动态提升 owner 优先级

- 用 sched_setscheduler() 恢复原始优先级

你可以直接参考 kernel/rtmutex.c 的实现方式。

------

## 4. 测试建议

- 可以写内核自测代码，或用内核自带的 rtmutex 测试用例做对比。

- 也可以用你之前的测试模块，直接调用 pri_mutex 接口。

------

## 5. 参考资料

- kernel/rtmutex.c

- include/linux/rtmutex.h

- Linux 内核模块开发手册

------

如需我帮你整理集成版 pri_mutex 代码（适合直接放进内核源码），请告诉我你希望的接口和功能细节，我可以帮你写出适合直接集成的代码！