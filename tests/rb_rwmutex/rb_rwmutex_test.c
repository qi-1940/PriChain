#include <linux/rb_rwmutex.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/spinlock.h>
#include <linux/rwlock.h>
#include <linux/completion.h>

// 测试模式：0=rb_rwmutex, 1=standard rwlock_t
static int test_mode = 0;
module_param(test_mode, int, 0644);
MODULE_PARM_DESC(test_mode, "Test mode: 0=rb_rwmutex, 1=standard rwlock_t");

// 测试用的锁
static struct rb_rwmutex rb_test_rwlock;
static rwlock_t standard_rwlock;

// 线程任务结构
static struct task_struct *low_prio_writer, *high_prio_writer, *interference_thread;

// 同步机制
static struct completion low_writer_has_lock;
//static struct completion high_writer_finished;
static struct completion high_writer_created;
static struct completion interference_created;
static struct completion interference_created_2;
static struct completion high_writer_created_2;

// 时延测试相关变量
DEFINE_SPINLOCK(time_measure_lock);
static ktime_t h_block_start_time;
static ktime_t h_lock_acquire_time;
static bool time_recorded = false;

struct sched_param {
    int sched_priority;
};

// 封装线程创建函数，绑定到CPU 0
static struct task_struct* create_and_start_thread(int (*threadfn)(void *data), 
                                                  const char *name, 
                                                  void *data) {
    struct task_struct *task;
    
    task = kthread_create(threadfn, data, name);
    if (IS_ERR(task)) {
        pr_err("[RWTEST-PI] Failed to create thread %s\n", name);
        return task;
    }
    
    kthread_bind(task, 0);  // 绑定到CPU 0
    wake_up_process(task);
    
    return task;
}

// 锁操作包装函数
void write_lock_wrapper(void) {
    switch (test_mode) {
        case 0: 
            rb_rwmutex_write_lock(&rb_test_rwlock); 
            break;
        case 1: 
            write_lock(&standard_rwlock); 
            break;
    }
}

void write_unlock_wrapper(void) {
    switch (test_mode) {
        case 0: 
            rb_rwmutex_write_unlock(&rb_test_rwlock); 
            break;
        case 1: 
            write_unlock(&standard_rwlock); 
            break;
    }
}

// 低优先级写线程：先获得锁，在临界区做长时间工作
static int low_prio_writer_fn(void *data)
{
    int i;
    struct sched_param sp = { .sched_priority = 10 };
    char *lock_names[] = {"rb_rwmutex", "standard rwlock_t"};
    
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    pr_alert("[RWTEST-PI-%s] Low Writer: START, prio=%d, normal_prio=%d\n", 
             lock_names[test_mode], current->prio, current->normal_prio);

    write_lock_wrapper();
    pr_alert("[RWTEST-PI-%s] Low Writer: GOT WRITE LOCK, prio=%d, normal_prio=%d\n", 
             lock_names[test_mode], current->prio, current->normal_prio);
    
    // 通知低优先级写线程已经获得锁
    complete(&low_writer_has_lock);
    schedule();
    //wait_for_completion(&high_writer_created);
    
    // 在临界区做CPU密集工作，期间观察优先级变化
    // 增加工作量，确保在高优先级线程创建时还在工作
    for (i = 0; i < 100000000; i++) {
        if (kthread_should_stop()) {
            pr_alert("[RWTEST-PI-%s] Low Writer: Received stop signal\n", lock_names[test_mode]);
            break;
        }
        if (i % 10000000 == 0) {
            pr_alert("[RWTEST-PI-%s] Low Writer: working iteration %d, prio=%d, normal_prio=%d\n", 
                     lock_names[test_mode], i, current->prio, current->normal_prio);
            schedule(); 
        }
        // 防止编译器优化掉循环，保持CPU密集
        __asm__ volatile("" : : : "memory");
    }

    if (kthread_should_stop()) {
        pr_alert("[RWTEST-PI-%s] Low Writer: Received stop signal before second phase\n", lock_names[test_mode]);
        goto unlock_and_exit;
    }
    
    wait_for_completion(&interference_created);
    wait_for_completion(&high_writer_created_2);
    schedule();

    for (i = 0; i < 100000000; i++) {
        if (kthread_should_stop()) {
            pr_alert("[RWTEST-PI-%s] Low Writer: Received stop signal in second loop\n", lock_names[test_mode]);
            break;
        }
        if (i % 10000000 == 0) {
            pr_alert("[RWTEST-PI-%s] Low Writer: working iteration %d, prio=%d, normal_prio=%d\n", 
                     lock_names[test_mode], i, current->prio, current->normal_prio);
            schedule(); 
        }
        // 防止编译器优化掉循环，保持CPU密集
        __asm__ volatile("" : : : "memory");
    }
    
unlock_and_exit:
    write_unlock_wrapper();
    pr_alert("[RWTEST-PI-%s] Low Writer: RELEASED WRITE LOCK, prio=%d, normal_prio=%d\n", 
             lock_names[test_mode], current->prio, current->normal_prio);
    
    pr_alert("[RWTEST-PI-%s] Low Writer: END, prio=%d, normal_prio=%d\n", 
             lock_names[test_mode], current->prio, current->normal_prio);
    
    return 0;
}

// 高优先级写线程：尝试获取写锁，会被阻塞，测试时延和优先级继承
static int high_prio_writer_fn(void *data)
{
    struct sched_param sp = { .sched_priority = 50 };
    char *lock_names[] = {"rb_rwmutex", "standard rwlock_t"};
    
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    pr_alert("[RWTEST-PI-%s] High Writer: START, prio=%d, normal_prio=%d\n", 
             lock_names[test_mode], current->prio, current->normal_prio);

    // 检查是否应该停止
    if (kthread_should_stop()) {
        pr_alert("[RWTEST-PI-%s] High Writer: Received stop signal before locking\n", lock_names[test_mode]);
        return 0;
    }

    // 时延测试：记录开始尝试获取锁的时间
    h_block_start_time = ktime_get();
    pr_alert("[RWTEST-PI-%s] High Writer: Attempting to acquire write lock at time: %lld ns\n", 
             lock_names[test_mode], ktime_to_ns(h_block_start_time));
    
    // 现在阻塞等待锁（这里应该发生优先级继承）
    write_lock_wrapper();
    
    // 记录获得锁的时间
    h_lock_acquire_time = ktime_get();
    pr_alert("[RWTEST-PI-%s] High Writer: GOT WRITE LOCK at time: %lld ns, prio=%d, normal_prio=%d\n", 
             lock_names[test_mode], ktime_to_ns(h_lock_acquire_time), current->prio, current->normal_prio);

    // 计算并显示阻塞时间
    if (!time_recorded) {
        ktime_t block_duration = ktime_sub(h_lock_acquire_time, h_block_start_time);
        pr_alert("[RWTEST-PI-%s] High Writer: Block duration: %lld ns (%lld us)\n", 
                 lock_names[test_mode], ktime_to_ns(block_duration), ktime_to_ns(block_duration) / 1000);
        time_recorded = true;
    }
    
    write_unlock_wrapper();
    pr_alert("[RWTEST-PI-%s] High Writer: RELEASED WRITE LOCK, prio=%d, normal_prio=%d\n", 
             lock_names[test_mode], current->prio, current->normal_prio);
    
    pr_alert("[RWTEST-PI-%s] High Writer: END, prio=%d, normal_prio=%d\n", 
             lock_names[test_mode], current->prio, current->normal_prio);
    
    return 0;
}

// 干扰线程：中等优先级，持续运行以观察优先级继承效果
static int interference_fn(void *data)
{
    int i = 0, j = 0;
    struct sched_param sp = { .sched_priority = 30 };
    char *lock_names[] = {"rb_rwmutex", "standard rwlock_t"};
    
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    pr_alert("[RWTEST-PI-%s] Interference: START, prio=%d, normal_prio=%d\n", 
             lock_names[test_mode], current->prio, current->normal_prio);
    
    for (i = 0; i < 50000000; i++) {
        if (kthread_should_stop()) {
            pr_alert("[RWTEST-PI-%s] Interference: Received stop signal in first loop\n", lock_names[test_mode]);
            break;
        }
        if (i % 10000000 == 0) {
            pr_alert("[RWTEST-PI-%s] Interference: PREEMPTING Phase 1! iteration %d, prio=%d\n", 
                     lock_names[test_mode], i, current->prio, current->normal_prio);
            schedule();
        }
        __asm__ volatile("" : : : "memory");
    }

    if (kthread_should_stop()) {
        pr_alert("[RWTEST-PI-%s] Interference: Received stop signal before second phase\n", lock_names[test_mode]);
        goto interference_exit;
    }

    wait_for_completion(&high_writer_created);
    schedule();

    for (i = 0; i < 50000000; i++) {
        if (kthread_should_stop()) {
            pr_alert("[RWTEST-PI-%s] Interference: Received stop signal in second loop\n", lock_names[test_mode]);
            break;
        }
        if (i % 10000000 == 0) {
            pr_alert("[RWTEST-PI-%s] Interference: PREEMPTING Phase 1! iteration %d, prio=%d\n", 
                     lock_names[test_mode], i, current->prio, current->normal_prio);
            schedule();
        }
        __asm__ volatile("" : : : "memory");
    }
    
interference_exit:
    pr_alert("[RWTEST-PI-%s] Interference: END, prio=%d, normal_prio=%d\n", 
             lock_names[test_mode], current->prio, current->normal_prio);
    
    return 0;
}

static int __init rwmutex_test_init(void)
{
    char *lock_names[] = {"rb_rwmutex", "standard rwlock_t"};
    struct sched_param sp;
    
    pr_info("[RWTEST-PI-%s] Priority inheritance and latency test loaded\n", lock_names[test_mode]);
    pr_info("[RWTEST-PI-%s] Testing with: %s\n", lock_names[test_mode], lock_names[test_mode]);
    
    // 设置主线程高优先级
    sp.sched_priority = 90;
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    
    // 初始化锁
    rb_rwmutex_init(&rb_test_rwlock);
    rwlock_init(&standard_rwlock);
    
    // 初始化同步机制
    init_completion(&low_writer_has_lock);
    //init_completion(&high_writer_finished);
    init_completion(&high_writer_created);
    init_completion(&interference_created);
    init_completion(&interference_created_2);
    init_completion(&high_writer_created_2);
    // 重置时间记录标志
    time_recorded = false;
    
    // 创建并启动低优先级写线程（绑定CPU0）
    low_prio_writer = create_and_start_thread(low_prio_writer_fn, "low_writer", NULL);
    
    // 等待低优先级写线程获得锁
    wait_for_completion(&low_writer_has_lock);
    
    // 创建并启动干扰线程（绑定CPU0）
    interference_thread = create_and_start_thread(interference_fn, "interference", NULL);
    wait_for_completion(&interference_created_2);
    
    high_prio_writer = create_and_start_thread(high_prio_writer_fn, "high_writer", NULL);
    
    return 0;
}

static void __exit rwmutex_test_exit(void)
{
    char *lock_names[] = {"rb_rwmutex", "standard rwlock_t"};
    
    pr_info("[RWTEST-PI-%s] Starting module cleanup...\n", lock_names[test_mode]);
    
    // 停止并等待所有线程结束
    if (low_prio_writer && !IS_ERR(low_prio_writer)) {
        kthread_stop(low_prio_writer);
        pr_info("[RWTEST-PI-%s] Low priority writer thread stopped\n", lock_names[test_mode]);
    }
    
    if (high_prio_writer && !IS_ERR(high_prio_writer)) {
        kthread_stop(high_prio_writer);
        pr_info("[RWTEST-PI-%s] High priority writer thread stopped\n", lock_names[test_mode]);
    }
    
    if (interference_thread && !IS_ERR(interference_thread)) {
        kthread_stop(interference_thread);
        pr_info("[RWTEST-PI-%s] Interference thread stopped\n", lock_names[test_mode]);
    }
    
    pr_info("[RWTEST-PI-%s] Priority inheritance and latency test unloaded\n", lock_names[test_mode]);
}

module_init(rwmutex_test_init);
module_exit(rwmutex_test_exit);
MODULE_LICENSE("GPL"); 