# 初始化和参数检查
```
if (!IS_ENABLED(CONFIG_FUTEX_PI))
    return -ENOSYS;
if (refill_pi_state_cache())
    return -ENOMEM;
```
## 功能​​
**检查内核是否启用PI Futex支持（CONFIG_FUTEX_PI），并预分配futex_pi_state缓存。若未启用或缓存不足，直接返回错误**

* 关键结构​​：futex_pi_state用于绑定用户态锁与内核rt_mutex，记录优先级继承信息

# 超时定时器初始化​
```
if (time) {
    hrtimer_init_on_stack(&to->timer, CLOCK_REALTIME, HRTIMER_MODE_ABS);
    hrtimer_set_expires(&to->timer, *time);
}
```
## 功能
**若调用指定超时时间（time参数），初始化高精度定时器，用于阻塞等待的限时控制**

# 获取Futex Key与哈希桶锁​
```
ret = get_futex_key(uaddr, flags & FLAGS_SHARED, &q.key, VERIFY_WRITE);
hb = queue_lock(&q);
```
* **get_futex_key​**​：通过用户地址uaddr生成唯一Key，用于定位内核的futex_hash_bucket（哈希桶）。该Key基于内存映射（mmap_sem）和地址对齐规则生成
* **queue_lock**​​：获取哈希桶自旋锁（hb->lock），确保后续操作的原子性。

# 原子操作尝试获取锁​
```
ret = futex_lock_pi_atomic(uaddr, hb, &q.key, &q.pi_state, current, &exiting, 0);
switch (ret) {
    case 1: // 成功获取锁
        goto out_unlock_put_key;
    case -EBUSY: // 锁被占用或有竞争
        queue_unlock(hb);
        wait_for_owner_exiting(ret, exiting);
        goto retry;
    // 其他错误处理...
}
```
* ​**futex_lock_pi_atomic​**​：通过原子指令（如cmpxchg）尝试将用户态锁值从0改为当前线程TID。若成功（返回1），直接返回；若失败，处理竞争或等待锁持有者退出。
* ​​**EBUSY处理​**​：若锁被占用（返回-EBUSY），释放哈希桶锁并等待持有者退出（如线程正在退出），随后重试

# 加入等待队列与代理锁​
```
__queue_me(&q, hb); // 将futex_q加入哈希桶等待队列
rt_mutex_init_waiter(&rt_waiter); // 初始化rt_mutex_waiter
ret = __rt_mutex_start_proxy_lock(&q.pi_state->pi_mutex, &rt_waiter, current);
```
* **__queue_me**​​：将当前线程的futex_q对象加入哈希桶的等待队列，关联用户态锁与内核rt_mutex。
* **rt_mutex_waiter​**​：表示线程阻塞在rt_mutex上，用于构建优先级继承链（PI Chain）。rt_mutex_start_proxy_lock将当前线程加入rt_mutex的等待队列，并触发优先级继承逻辑。

# 阻塞等待与超时处理
```
ret = rt_mutex_wait_proxy_lock(&q.pi_state->pi_mutex, to, &rt_waiter);
if (ret) {
    rt_mutex_cleanup_proxy_lock(&q.pi_state->pi_mutex, &rt_waiter);
}
```

* **​​rt_mutex_wait_proxy_lock**​​：线程阻塞在rt_mutex上，直到被唤醒或超时。若超时或中断（如信号），调用rt_mutex_cleanup_proxy_lock清理代理锁状态。
* **优先级继承​​**：在此期间，若高优先级线程被阻塞，内核会提升当前锁持有者的优先级，避免优先级反转。

# 修正锁状态与资源释放​
```
res = fixup_owner(uaddr, &q, !ret); // 修正锁的拥有者
unqueue_me_pi(&q); // 从等待队列移除
queue_unlock(hb); // 释放哈希桶锁
put_futex_key(&q.key); // 释放Futex Key
```
* **fixup_owner**​​：若锁状态不一致（如持有者意外退出），修正用户态锁值为新持有者的TID，确保后续唤醒正确。
* **​资源释放**​​：清理等待队列、哈希桶锁和Futex Key，防止资源泄漏。

# 关键数据结构与机制
* **futex_q**​​：表示一个阻塞在用户态锁上的线程，关联futex_pi_state和rt_mutex。
* **rt_mutex​​**：内核实时互斥锁，管理线程阻塞队列和优先级继承。
* **优先级继承链 PI Chain**​​：通过rt_mutex_waiter和futex_pi_state跟踪阻塞关系，动态调整线程优先级。