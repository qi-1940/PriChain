**owner 状态的原子管理、waiters 标志的维护、并发下的正确性保障**

* rt_mutex_set_owner

* clear_rt_mutex_waiters

* fixup_rt_mutex_waiters

 * rt_mutex_real_waiter

 * rt_mutex_cmpxchg_relaxed 等宏

 * mark_rt_mutex_waiters

 * unlock_rt_mutex_safe

**确定了接下来采用内核集成的办法，先研究rtmutex的实现，再考虑后续进行rwmutex和rwsem的写锁以及mutex的优先级继承依靠rtmutex**

**边实现边测案例，最后写宏定义;主要用别人的成果来对比，查漏补缺**

