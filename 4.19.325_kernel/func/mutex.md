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
__mutex_lock_common()
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

#### 1.乐观自旋
```
static bool mutex_optimistic_spin(struct mutex *lock, ...) {
    // 检查是否可以自旋（持有者在CPU上且未抢占）
    if (!mutex_can_spin_on_owner(lock)) goto fail;
    // 获取MCS锁以避免多个任务同时自旋
    if (!osq_lock(&lock->osq)) goto fail;
    // 自旋等待锁释放
    while (__mutex_owner(lock) == owner) {
        if (need_resched() || !owner->on_cpu) break;
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

#### 3.阻塞与调度
```
set_current_state(TASK_UNINTERRUPTIBLE);
for (;;) {
    if (__mutex_trylock(lock)) break; // 再次尝试获取锁
    spin_unlock(&lock->wait_lock);
    schedule_preempt_disabled(); // 主动让出CPU
    spin_lock(&lock->wait_lock);
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

