# rtmutex 优先级继承机制的基础设施
## 负责 owner 状态的原子管理、waiters 标志的维护、并发下的正确性保障

```
/*
 * lock->owner state tracking:
 *
 * lock->owner holds the task_struct pointer of the owner. Bit 0
 * is used to keep track of the "lock has waiters" state.
 *
 * owner	bit0
 * NULL		0	lock is free (fast acquire possible)
 * NULL		1	lock is free and has waiters and the top waiter
 *				is going to take the lock*
 * taskpointer	0	lock is held (fast release possible)
 * taskpointer	1	lock is held and has waiters**
 *
 * The fast atomic compare exchange based acquire and release is only
 * possible when bit 0 of lock->owner is 0.
 *
 * (*) It also can be a transitional state when grabbing the lock
 * with ->wait_lock is held. To prevent any fast path cmpxchg to the lock,
 * we need to set the bit0 before looking at the lock, and the owner may be
 * NULL in this small time, hence this can be a transitional state.
 *
 * (**) There is a small time when bit 0 is set but there are no
 * waiters. This can happen when grabbing the lock in the slow path.
 * To prevent a cmpxchg of the owner releasing the lock, we need to
 * set this bit before looking at the lock.
 */

///设置 rtmutex 的 owner 字段，记录当前持有锁的任务，并在 owner 指针的最低位（bit 0）标记“是否有等待者”
static void
rt_mutex_set_owner(struct rt_mutex *lock, struct task_struct *owner)
{
// 将 owner 指针转为 long 类型，便于后续位操作
	unsigned long val = (unsigned long)owner;
// #define RT_MUTEX_HAS_WAITERS	1UL
	if (rt_mutex_has_waiters(lock))
		val |= RT_MUTEX_HAS_WAITERS;
// 将 owner 和 waiters 标志一起写入 owner 字段
	lock->owner = (struct task_struct *)val;
}

///清除 owner 字段的“有等待者”标志（bit 0）
static inline void clear_rt_mutex_waiters(struct rt_mutex *lock)
{
// 将 lock->owner 的值转换为 unsigned long 类型，便于位操作
	lock->owner = (struct task_struct *)
			((unsigned long)lock->owner & ~RT_MUTEX_HAS_WAITERS);
}

///在没有等待者时，确保 owner 字段的 waiters 标志被清除，防止并发竞态导致 owner 状态不一致	
static void fixup_rt_mutex_waiters(struct rt_mutex *lock)
{
// 如果锁的等待者树中没有等待者，则清除 owner 字段的“有等待者”标志
	unsigned long owner, *p = (unsigned long *) &lock->owner;
// 如果锁的等待者树中有等待者，则不需要进行任何操作
	if (rt_mutex_has_waiters(lock))
		return;

	/*
	 * The rbtree has no waiters enqueued, now make sure that the
	 * lock->owner still has the waiters bit set, otherwise the
	 * following can happen:
	 *
	 * CPU 0	CPU 1		CPU2
	 * l->owner=T1
	 *		rt_mutex_lock(l)
	 *		lock(l->lock)
	 *		l->owner = T1 | HAS_WAITERS;
	 *		enqueue(T2)
	 *		boost()
	 *		  unlock(l->lock)
	 *		block()
	 *
	 *				rt_mutex_lock(l)
	 *				lock(l->lock)
	 *				l->owner = T1 | HAS_WAITERS;
	 *				enqueue(T3)
	 *				boost()
	 *				  unlock(l->lock)
	 *				block()
	 *		signal(->T2)	signal(->T3)
	 *		lock(l->lock)
	 *		dequeue(T2)
	 *		deboost()
	 *		  unlock(l->lock)
	 *				lock(l->lock)
	 *				dequeue(T3)
	 *				 ==> wait list is empty
	 *				deboost()
	 *				 unlock(l->lock)
	 *		lock(l->lock)
	 *		fixup_rt_mutex_waiters()
	 *		  if (wait_list_empty(l) {
	 *		    l->owner = owner
	 *		    owner = l->owner & ~HAS_WAITERS;
	 *		      ==> l->owner = T1
	 *		  }
	 *				lock(l->lock)
	 * rt_mutex_unlock(l)		fixup_rt_mutex_waiters()
	 *				  if (wait_list_empty(l) {
	 *				    owner = l->owner & ~HAS_WAITERS;
	 * cmpxchg(l->owner, T1, NULL)
	 *  ===> Success (l->owner = NULL)
	 *
	 *				    l->owner = owner
	 *				      ==> l->owner = T1
	 *				  }
	 *
	 * With the check for the waiter bit in place T3 on CPU2 will not
	 * overwrite. All tasks fiddling with the waiters bit are
	 * serialized by l->lock, so nothing else can modify the waiters
	 * bit. If the bit is set then nothing can change l->owner either
	 * so the simple RMW is safe. The cmpxchg() will simply fail if it
	 * happens in the middle of the RMW because the waiters bit is
	 * still set.
	 */
// 原子读取 owner 字段
	owner = READ_ONCE(*p);
// 如果 owner 字段的最低位（bit 0）被设置，说明有等待者
	if (owner & RT_MUTEX_HAS_WAITERS)
		WRITE_ONCE(*p, owner & ~RT_MUTEX_HAS_WAITERS);
}

///判断 waiter 是否为“真实等待者”
static int rt_mutex_real_waiter(struct rt_mutex_waiter *waiter)
{
// 返回等待者指针,过滤掉特殊的“唤醒中”或“重排中”标志,只对真正阻塞在锁上的 waiter 进行优先级继承等操作，忽略中间态
	return waiter && waiter != PI_WAKEUP_INPROGRESS &&
		waiter != PI_REQUEUE_INPROGRESS;
}

/*
 * We can speed up the acquire/release, if there's no debugging state to be
 * set up.
 */
///用于原子地比较并交换 owner 字段，支持 fastpath;在非 debug 模式下，使用高效的原子操作实现加锁/解锁
#ifndef CONFIG_DEBUG_RT_MUTEXES
# define rt_mutex_cmpxchg_relaxed(l,c,n) (cmpxchg_relaxed(&l->owner, c, n) == c)
# define rt_mutex_cmpxchg_acquire(l,c,n) (cmpxchg_acquire(&l->owner, c, n) == c)
# define rt_mutex_cmpxchg_release(l,c,n) (cmpxchg_release(&l->owner, c, n) == c)

/*
 * Callers must hold the ->wait_lock -- which is the whole purpose as we force
 * all future threads that attempt to [Rmw] the lock to the slowpath. As such
 * relaxed semantics suffice.
 */
///原子地设置 owner 字段的 waiters 标志;保证并发下 waiters 标志的正确设置，防止竞态
static inline void mark_rt_mutex_waiters(struct rt_mutex *lock)
{
	unsigned long owner, *p = (unsigned long *) &lock->owner;

	do {
		owner = *p;
	} while (cmpxchg_relaxed(p, owner,
				 owner | RT_MUTEX_HAS_WAITERS) != owner);
}

/*
 * Safe fastpath aware unlock:
 * 1) Clear the waiters bit
 * 2) Drop lock->wait_lock
 * 3) Try to unlock the lock with cmpxchg
 */
///安全地解锁 rt_mutex，清除 waiters 标志，并尝试原子释放 owner;处理 fastpath 和 slowpath 的竞态，保证解锁的原子性和正确性
static inline bool unlock_rt_mutex_safe(struct rt_mutex *lock,
					unsigned long flags)
	__releases(lock->wait_lock)
{
	struct task_struct *owner = rt_mutex_owner(lock);

	clear_rt_mutex_waiters(lock);
	raw_spin_unlock_irqrestore(&lock->wait_lock, flags);
	/*
	 * If a new waiter comes in between the unlock and the cmpxchg
	 * we have two situations:
	 *
	 * unlock(wait_lock);
	 *					lock(wait_lock);
	 * cmpxchg(p, owner, 0) == owner
	 *					mark_rt_mutex_waiters(lock);
	 *					acquire(lock);
	 * or:
	 *
	 * unlock(wait_lock);
	 *					lock(wait_lock);
	 *					mark_rt_mutex_waiters(lock);
	 *
	 * cmpxchg(p, owner, 0) != owner
	 *					enqueue_waiter();
	 *					unlock(wait_lock);
	 * lock(wait_lock);
	 * wake waiter();
	 * unlock(wait_lock);
	 *					lock(wait_lock);
	 *					acquire(lock);
	 */
	return rt_mutex_cmpxchg_release(lock, owner, NULL);
}
```
## 接下来是主要具体实现部分
rt_mutex_waiter_less、rt_mutex_enqueue、rt_mutex_adjust_prio_chain 等