# 互斥锁mutex

### __mutex_init()

* 初始化互斥锁
    * 设置owner为0
    * 初始化自旋锁和等待队列
    * 如果启用 CONFIG_MUTEX_SPIN_ON_OWNER，初始化乐观自旋队列（OSQ）
    * 调用调试初始化函数（如启用调试）
```
    void
    __mutex_init(struct mutex *lock, const char *name, struct lock_class_key *key)
    {
        atomic_long_set(&lock->owner, 0);
        spin_lock_init(&lock->wait_lock);
        INIT_LIST_HEAD(&lock->wait_list);
    #ifdef CONFIG_MUTEX_SPIN_ON_OWNER
        osq_lock_init(&lock->osq);
    #endif

	debug_mutex_init(lock, name, key);
    }
```

### mutex_lock()

* 获取互斥锁(阻塞版本)
    * 尝试快速路径（无竞争时直接获取锁）
    * 失败则进入慢路径 __mutex_lock_slowpath，可能阻塞或自旋等待
```
    #ifndef CONFIG_DEBUG_LOCK_ALLOC
    static void __sched __mutex_lock_slowpath(struct mutex *lock);
    void __sched mutex_lock(struct mutex *lock)
    {
        might_sleep();

        if (!__mutex_trylock_fast(lock))
            __mutex_lock_slowpath(lock);
    }
    EXPORT_SYMBOL(mutex_lock);
    #endif
```
### __mutex_trylock_fast()

快速路径​​（__mutex_trylock_fast）：无竞争时直接通过原子操作获取锁

atomic_long_try_cmpxchg_acquire检查lock->owner是否为0

若成功，设置owner为当前任务指针，返回true（加锁成功）

若失败（存在竞争），进入慢路径

    ```
        static __always_inline bool __mutex_trylock_fast(struct mutex *lock)
    {
        unsigned long curr = (unsigned long)current;    //在多任务系统中，current 用于标识哪个进程正在持有 CPU 资源
        unsigned long zero = 0UL;   //用于表示锁的“空闲状态”（即当前没有任务持有锁）
        // 原子操作：若当前锁的owner为0（未持有），则设置为当前任务指针
        if (atomic_long_try_cmpxchg_acquire(&lock->owner, &zero, curr))
            return true;

        return false;
    }
    ```
### __mutex_lock_slowpath() 

慢路径​​（__mutex_lock_slowpath）：处理锁竞争，包括加入等待队列、自旋等待、优先级继承（需扩展）

__mutex_lock_slowpath() → __mutex_lock()

```
    static noinline void __sched
    __mutex_lock_slowpath(struct mutex *lock)
    {
        __mutex_lock(lock, TASK_UNINTERRUPTIBLE, 0, NULL, _RET_IP_);
    }
```
__mutex_lock() → __mutex_lock_common()
```
static int __sched
__mutex_lock(struct mutex *lock, long state, unsigned int subclass,
	     struct lockdep_map *nest_lock, unsigned long ip)
{
	return __mutex_lock_common(lock, state, subclass, nest_lock, ip, NULL, false);
}
```
__mutex_lock_common() **不重要!!!不需详细读**
```
    static __always_inline int __sched
    __mutex_lock_common(struct mutex *lock, long state, unsigned int subclass,
                struct lockdep_map *nest_lock, unsigned long ip,
                struct ww_acquire_ctx *ww_ctx, const bool use_ww_ctx)
    {
        struct mutex_waiter waiter;
        struct ww_mutex *ww;
        int ret;

        if (!use_ww_ctx)
            ww_ctx = NULL;

        might_sleep();

        ww = container_of(lock, struct ww_mutex, base);
        if (ww_ctx) {
            if (unlikely(ww_ctx == READ_ONCE(ww->ctx)))
                return -EALREADY;

            /*
            * Reset the wounded flag after a kill. No other process can
            * race and wound us here since they can't have a valid owner
            * pointer if we don't have any locks held.
            */
            if (ww_ctx->acquired == 0)
                ww_ctx->wounded = 0;
        }

        preempt_disable();
        mutex_acquire_nest(&lock->dep_map, subclass, 0, nest_lock, ip);

        if (__mutex_trylock(lock) ||
            mutex_optimistic_spin(lock, ww_ctx, NULL)) {
            /* got the lock, yay! */
            lock_acquired(&lock->dep_map, ip);
            if (ww_ctx)
                ww_mutex_set_context_fastpath(ww, ww_ctx);
            preempt_enable();
            return 0;
        }

        spin_lock(&lock->wait_lock);
        /*
        * After waiting to acquire the wait_lock, try again.
        */
        if (__mutex_trylock(lock)) {
            if (ww_ctx)
                __ww_mutex_check_waiters(lock, ww_ctx);

            goto skip_wait;
        }

        debug_mutex_lock_common(lock, &waiter);

        lock_contended(&lock->dep_map, ip);

        if (!use_ww_ctx) {
            /* add waiting tasks to the end of the waitqueue (FIFO): */
            __mutex_add_waiter(lock, &waiter, &lock->wait_list);


    #ifdef CONFIG_DEBUG_MUTEXES
            waiter.ww_ctx = MUTEX_POISON_WW_CTX;
    #endif
        } else {
            /*
            * Add in stamp order, waking up waiters that must kill
            * themselves.
            */
            ret = __ww_mutex_add_waiter(&waiter, lock, ww_ctx);
            if (ret)
                goto err_early_kill;

            waiter.ww_ctx = ww_ctx;
        }

        waiter.task = current;

        set_current_state(state);
        for (;;) {
            bool first;

            /*
            * Once we hold wait_lock, we're serialized against
            * mutex_unlock() handing the lock off to us, do a trylock
            * before testing the error conditions to make sure we pick up
            * the handoff.
            */
            if (__mutex_trylock(lock))
                goto acquired;

            /*
            * Check for signals and kill conditions while holding
            * wait_lock. This ensures the lock cancellation is ordered
            * against mutex_unlock() and wake-ups do not go missing.
            */
            if (unlikely(signal_pending_state(state, current))) {
                ret = -EINTR;
                goto err;
            }

            if (ww_ctx) {
                ret = __ww_mutex_check_kill(lock, &waiter, ww_ctx);
                if (ret)
                    goto err;
            }

            spin_unlock(&lock->wait_lock);
            schedule_preempt_disabled();

            first = __mutex_waiter_is_first(lock, &waiter);
            if (first)
                __mutex_set_flag(lock, MUTEX_FLAG_HANDOFF);

            set_current_state(state);
            /*
            * Here we order against unlock; we must either see it change
            * state back to RUNNING and fall through the next schedule(),
            * or we must see its unlock and acquire.
            */
            if (__mutex_trylock(lock) ||
                (first && mutex_optimistic_spin(lock, ww_ctx, &waiter)))
                break;

            spin_lock(&lock->wait_lock);
        }
        spin_lock(&lock->wait_lock);
    acquired:
        __set_current_state(TASK_RUNNING);

        if (ww_ctx) {
            /*
            * Wound-Wait; we stole the lock (!first_waiter), check the
            * waiters as anyone might want to wound us.
            */
            if (!ww_ctx->is_wait_die &&
                !__mutex_waiter_is_first(lock, &waiter))
                __ww_mutex_check_waiters(lock, ww_ctx);
        }

        __mutex_remove_waiter(lock, &waiter);

        debug_mutex_free_waiter(&waiter);

    skip_wait:
        /* got the lock - cleanup and rejoice! */
        lock_acquired(&lock->dep_map, ip);

        if (ww_ctx)
            ww_mutex_lock_acquired(ww, ww_ctx);

        spin_unlock(&lock->wait_lock);
        preempt_enable();
        return 0;

    err:
        __set_current_state(TASK_RUNNING);
        __mutex_remove_waiter(lock, &waiter);
    err_early_kill:
        spin_unlock(&lock->wait_lock);
        debug_mutex_free_waiter(&waiter);
        mutex_release(&lock->dep_map, 1, ip);
        preempt_enable();
        return ret;
    }
```

以上不需要详细了解

#### 1.乐观自旋
```
static bool mutex_optimistic_spin(struct mutex *lock, ...) {
    // 检查是否可以自旋（持有者在CPU上且未抢占）
    if (!mutex_can_spin_on_owner(lock)) goto fail;
    // 获取MCS锁以避免多个任务同时自旋
    if (!osq_lock(&lock->osq)) goto fail;
    // 自旋等待锁释放,owner 是之前通过 __mutex_owner(lock) 或 mutex_can_spin_on_owner() 获取的 ​​预期持有者指针​​
    while (__mutex_owner(lock) == owner) {
        if (need_resched() || !owner->on_cpu) break;//当前线程需要调度或owner不再占有cpu
        cpu_relax();
    }
    osq_unlock(&lock->osq);
}
```

#### 2.加入等待队列
```
static void __mutex_add_waiter(struct mutex *lock, struct mutex_waiter *waiter) {
    list_add_tail(&waiter->list, &lock->wait_list); // 加入队列尾部
    if (__mutex_waiter_is_first(lock, waiter))
        __mutex_set_flag(lock, MUTEX_FLAG_WAITERS); // 设置WAITERS标志
}
```
__mutex_waiter_is_first

判断当前 waiter 是否是等待队列中的​​第一个有效等待者​​（即队列此前为空）

通过 list_first_entry 获取队列首节点，与当前 waiter 比较
```
static inline bool __mutex_waiter_is_first(struct mutex *lock, struct mutex_waiter *waiter) {
    return list_first_entry(&lock->wait_list, struct mutex_waiter, list) == waiter;
}
```
__mutex_set_flag

atomic_long_or 原子设置标志位，避免竞争条件

#define MUTEX_FLAG_WAITERS 0x01：表示锁有等待者，释放锁时需唤醒队列
```
static inline void __mutex_set_flag(struct mutex *lock, unsigned long flag)
{
	atomic_long_or(flag, &lock->owner);
}
```

#### 阻塞与调度
```
set_current_state(TASK_UNINTERRUPTIBLE);
for (;;) {
    if (__mutex_trylock(lock)) break; // 再次尝试获取锁
    spin_unlock(&lock->wait_lock);
    //让当前线程主动放弃 CPU，进入睡眠状态，直到被唤醒（如锁释放时）
    schedule_preempt_disabled(); // 主动让出CPU
    spin_lock(&lock->wait_lock);
}
```
schedule_preempt_disabled
* preempt_disable 的意义​​：
    * 确保在调用 schedule() 前，当前线程不会被其他高优先级线程抢占。
    * 保护调度过程的原子性（例如避免在释放锁和调度之间插入其他操作）。
```
void schedule_preempt_disabled(void) {
    preempt_disable();  // 禁用内核抢占
    schedule();         // 主动触发调度
    preempt_enable();   // 重新启用抢占
}
```

### 总结：

```
mutex_lock()
├── __mutex_trylock_fast()         [快速路径尝试]
│   └── atomic_long_try_cmpxchg_acquire()
│       └── 成功: 获得锁，返回
└── __mutex_lock_slowpath()        [慢路径]
    └── __mutex_lock_common()
        ├── mutex_optimistic_spin() [乐观自旋]
        │   ├── osq_lock()          [MCS锁]
        │   └── mutex_spin_on_owner() [自旋等待]
        └── __mutex_add_waiter()   [加入等待队列]
            └── schedule_preempt_disabled() [阻塞]

mutex_unlock()
├── __mutex_unlock_fast()          [快速路径释放]
│   └── atomic_long_cmpxchg_release()
│       └── 成功: 释放锁，返回
└── __mutex_unlock_slowpath()      [慢路径]
    ├── __mutex_handoff()          [锁传递]
    └── wake_up_process()          [唤醒等待任务]
```

# 优先级继承

## 1. 优先级继承的核心函数

### (1) `__ww_mutex_check_waiters`

```
static void __sched
__ww_mutex_check_waiters(struct mutex *lock, struct ww_acquire_ctx *ww_ctx)
{
	struct mutex_waiter *cur;

	lockdep_assert_held(&lock->wait_lock);

	list_for_each_entry(cur, &lock->wait_list, list) {
		if (!cur->ww_ctx)
			continue;

		if (__ww_mutex_die(lock, cur, ww_ctx) ||
		    __ww_mutex_wound(lock, cur->ww_ctx, ww_ctx))
			break;
	}
}
```

**作用**  
检查等待队列中的线程，并根据优先级继承规则调整锁持有者的优先级。

**实现逻辑**  
- 遍历等待队列中的线程(`list_for_each_entry`)，判断是否需要触发优先级继承或优先级反转处理
- 调用`__ww_mutex_die`(处理"等待-死亡"策略)或`__ww_mutex_wound`(处理"伤害-等待"策略)，确保高优先级线程不会被低优先级线程无限阻塞

**触发场景**  
当锁被释放或线程加入等待队列时调用

### (2) `__ww_mutex_wound`

```
static bool __ww_mutex_wound(struct mutex *lock,
			     struct ww_acquire_ctx *ww_ctx,
			     struct ww_acquire_ctx *hold_ctx)
{
	struct task_struct *owner = __mutex_owner(lock);

	lockdep_assert_held(&lock->wait_lock);

	if (!hold_ctx)
		return false;

	if (!owner)
		return false;

	if (ww_ctx->acquired > 0 && __ww_ctx_stamp_after(hold_ctx, ww_ctx)) {
		hold_ctx->wounded = 1;

		if (owner != current)
			wake_up_process(owner);

		return true;
	}

	return false;
}
```
**作用**  
在"伤害-等待"模式下，若当前线程优先级高于锁持有者，强制让锁持有者提升优先级

**实现逻辑**  
- 通过`__ww_ctx_stamp_after`比较线程优先级时间戳
- 若当前线程优先级更高，则标记锁持有者为"受伤"(`hold_ctx->wounded = 1`)
- 并唤醒其重新调度

**触发场景**  
在锁竞争时由`__ww_mutex_check_waiters`调用

### (3) `__mutex_handoff`
```
static void __mutex_handoff(struct mutex *lock, struct task_struct *task)
{
	unsigned long owner = atomic_long_read(&lock->owner);

	for (;;) {
		unsigned long old, new;

#ifdef CONFIG_DEBUG_MUTEXES
		DEBUG_LOCKS_WARN_ON(__owner_task(owner) != current);
		DEBUG_LOCKS_WARN_ON(owner & MUTEX_FLAG_PICKUP);
#endif

		new = (owner & MUTEX_FLAG_WAITERS);
		new |= (unsigned long)task;
		if (task)
			new |= MUTEX_FLAG_PICKUP;

		old = atomic_long_cmpxchg_release(&lock->owner, owner, new);
		if (old == owner)
			break;

		owner = old;
	}
}
```
**作用**  
在锁释放时，将锁传递给等待队列中优先级最高的线程，并更新标志位(`MUTEX_FLAG_HANDOFF`)

**实现逻辑**  
- 通过原子操作(`atomic_long_cmpxchg_release`)设置新锁持有者
- 并调整其优先级

**触发场景**  
在`mutex_unlock`的慢路径中调用