用cursor做了使用内核锁rtmutex验证其实现的优先级继承功能
本实验通过三个不同优先级的内核线程（L低、M中、H高）竞争同一个rtmutex锁，完整展示了优先级继承机制：L线程先获得锁并开始工作，H线程随后请求锁被阻塞，M线程也尝试获取锁。此时，L线程的实际优先级被提升到与H相同，避免被M抢占，直到L释放锁后，H立即获得锁，最后M获得锁。整个过程中，日志详细记录了每个线程的启动、锁操作、优先级变化和结束，清晰验证了rtmutex优先级继承有效防止优先级反转

后续规划：
```
四、优先级继承的关键点
        rtmutex 通过 plist_head 维护优先级队列，阻塞线程按优先级排序。
    当高优先级线程阻塞在锁上时，rt_mutex_adjust_prio_chain 会提升 owner 的优先级，防止优先级反转。
        owner 释放锁后，优先级恢复。
五、调度器相关
        kernel/sched/core.c
        rt_mutex_setprio(struct task_struct *p, int prio)
    负责实际提升/恢复任务优先级。
```
**考虑集成rtmutex到mutex**
* 在 struct mutex 中增加 struct rt_mutex 成员，或直接用 struct rt_mutex 替换实现
* 在 slowpath 中，调用 rt_mutex_lock/rt_mutex_unlock，让 rtmutex 机制接管优先级继承
* plist_head 替换原有的 list_head，以支持优先级排序。
* 需要维护 owner 指针，便于优先级提升。
* 阅读
include/linux/mutex.h：理解 mutex 的接口和结构体。
kernel/locking/mutex.c：理解 mutex 的加锁/解锁流程，重点看 slowpath。
include/linux/rtmutex.h：理解 rtmutex 的接口和结构体。
kernel/locking/rtmutex.c：理解优先级继承的实现，重点看 rt_mutex_lock、rt_mutex_unlock、rt_mutex_adjust_prio_chain。
kernel/sched/core.c：理解调度器如何处理优先级提升。
