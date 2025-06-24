# 初始化和权限校验​
```
if (!IS_ENABLED(CONFIG_FUTEX_PI))
    return -ENOSYS;
if (get_user(uval, uaddr))
    return -EFAULT;
if ((uval & FUTEX_TID_MASK) != vpid)
    return -EPERM;
```
## 功能
* 检查内核是否启用PI Futex支持（CONFIG_FUTEX_PI）。
* 从用户空间读取锁值uval，验证当前进程是否为锁的持有者（通过FUTEX_TID_MASK匹配进程的虚拟PID vpid）

# 获取Futex Key与哈希桶锁​
```
ret = get_futex_key(uaddr, flags & FLAGS_SHARED, &key, VERIFY_WRITE);
hb = hash_futex(&key);
spin_lock(&hb->lock); //获取哈希桶自旋锁，确保后续操作原子性
```

# 处理等待队列​
```
top_waiter = futex_top_waiter(hb, &key);
if (top_waiter) {
    struct futex_pi_state *pi_state = top_waiter->pi_state;
    if (pi_state->owner != current)
        goto out_unlock;
    raw_spin_lock_irq(&pi_state->pi_mutex.wait_lock);
    spin_unlock(&hb->lock);
    ret = wake_futex_pi(uaddr, uval, pi_state);
}
```
## 关键逻辑​​：
* ​**​futex_top_waiter​**​：找到哈希桶中优先级最高的等待线程（top_waiter）。
* ​**所有权校验​​**：确保当前进程是pi_state的合法拥有者，防止恶意解锁。
* ​**代理锁操作**​​：
    * 获取rt_mutex的等待锁（wait_lock），释放哈希桶锁。
    *  wake_futex_pi​​：唤醒优先级最高的等待线程，传递锁所有权，并触发优先级继承链的调整

# 无等待者的快速路径​
```
if ((ret = cmpxchg_futex_value_locked(&curval, uaddr, uval, 0))) {
    spin_unlock(&hb->lock);
    // 处理错误：页面错误（-EFAULT）或重试（-EAGAIN）
}
```
## 功能
* 若无等待线程，直接通过原子操作cmpxchg将用户态锁值置0
* 若用户态锁值已被修改（curval != uval），返回-EAGAIN让用户态重试

# 错误处理与重试​
```
pi_retry:
    put_futex_key(&key);
    cond_resched();
    goto retry;

pi_faulted:
    put_futex_key(&key);
    ret = fault_in_user_writeable(uaddr);
    if (!ret)
        goto retry;
```
​​
* **pi_retry​**​：在-EAGAIN时释放资源并调度重试，避免活锁
* ​​**pi_faulted**​​：若用户地址不可写（-EFAULT），尝试修复页表后重试

# 关键数据结构与机制
* ​**​futex_pi_state**
    * 绑定用户态锁与内核rt_mutex，记录优先级继承状态
    * 在wake_futex_pi中更新owner字段，触发新所有者的优先级调整
* **rt_mutex代理机制**
    * 通过rt_mutex_waiter管理等待队列，维护优先级继承链（PI Chain）
    * 唤醒时调用rt_mutex_proxy_unlock传递锁所有权