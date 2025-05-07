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
    __mutex_trylock_fast()

    快速路径​​（__mutex_trylock_fast）：无竞争时直接通过原子操作获取锁
    ```
        static __always_inline bool __mutex_trylock_fast(struct mutex *lock)
    {
        unsigned long curr = (unsigned long)current;
        unsigned long zero = 0UL;

        if (atomic_long_try_cmpxchg_acquire(&lock->owner, &zero, curr))
            return true;

        return false;
    }
    ```
    __mutex_lock_slowpath()

    慢路径​​（__mutex_lock_slowpath）：处理锁竞争，包括加入等待队列、自旋等待、优先级继承（需扩展）
    ```
    static noinline void __sched
    __mutex_lock_slowpath(struct mutex *lock)
    {
        __mutex_lock(lock, TASK_UNINTERRUPTIBLE, 0, NULL, _RET_IP_);
    }
    ```
