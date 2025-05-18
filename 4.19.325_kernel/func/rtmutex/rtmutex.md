# 实时互斥锁rt-mutex
**FUTEX_LOCK_PI 依赖 ​​RT-Mutex（实时互斥锁）​​ 实现优先级继承**
* rt_mutex_lock()：获取 PI 锁的核心函数。
* rt_mutex_unlock()：释放 PI 锁并调整优先级。
* struct rt_mutex：PI 锁的内核表示。
* struct futex_pi_state：关联 Futex 和 RT-Mutex 的中间结构   