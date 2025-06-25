# Linux 内核中的 `rtmutex` 实现解析

## 1. `rtmutex` 在内核中的位置

### 1.1 源代码文件
- ​**路径**: `kernel/locking/rtmutex.c`
- ​**功能**: 实现实时互斥锁（Real-Time Mutex）的核心逻辑，包括锁的获取、释放、优先级继承（Priority Inheritance）等机制。

### 1.2 头文件
- ​**路径**: `include/linux/rtmutex.h`
- ​**功能**: 定义 `rt_mutex` 结构体和相关操作函数的声明。

---

## 2. 优先级继承（Priority Inheritance）关键函数

### 2.1 核心函数列表

| 函数名                     | 作用                                                                                     |
|--------------------------|----------------------------------------------------------------------------------------|
| `rt_mutex_adjust_prio_chain` | 递归遍历因优先级继承而需要调整的任务链，解决嵌套锁导致的优先级反转问题。                                   |
| `rt_mutex_adjust_prio`     | 调整任务的优先级（包括继承后的优先级），并触发调度器重新评估任务状态。                                    |
| `rt_mutex_setprio`         | 直接设置任务的实时优先级（`task->prio`），并通知调度器（调用 `set_current_task_prio`）。                 |
| `rt_mutex_enqueue` / `rt_mutex_dequeue` | 将任务加入/移出互斥锁的等待队列，并根据优先级排序（高优先级任务在前）。                             |
| `adjust_waiters_prio`      | 当任务优先级变化时，调整其等待队列中所有依赖此任务的阻塞任务的优先级（级联更新）。                             |

### 2.2 调用优先级继承的上下文函数

| 函数名            | 关联操作                                                                                   |
|-----------------|------------------------------------------------------------------------------------------|
| `rt_mutex_lock`   | 尝试获取锁时，若锁被占用，触发优先级继承（调用 `rt_mutex_adjust_prio_chain`）。                              |
| `rt_mutex_unlock` | 释放锁时，检查是否有等待者需要继承优先级（调用 `rt_mutex_adjust_prio_chain` 恢复原优先级）。                       |
| `rt_mutex_trylock` | 非阻塞尝试获取锁，失败时可能触发优先级调整。                                                           |

---

## 3. 代码片段示例

### 3.1 优先级继承链递归调整
```c
// rt_mutex_adjust_prio_chain 核心逻辑片段
while (task) {
    next_task = rt_mutex_top_waiter(task->pi_blocked_on)->task;
    rt_mutex_adjust_prio(task);  // 递归调整优先级
    task = next_task;
}