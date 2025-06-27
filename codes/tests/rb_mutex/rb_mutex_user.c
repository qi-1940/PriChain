#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/rtmutex.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/delay.h>
#include <linux/cpumask.h>
#include <linux/ktime.h>
#include <linux/completion.h>
#include "rb_mutex.h"
#include <linux/kthread.h>

struct sched_param {
    int sched_priority;
};

static struct rb_mutex test_lock;
DEFINE_SPINLOCK(time_measure_lock);
static struct task_struct *task_l, *task_h, *task_m;
static ktime_t h_block_start_time;
static ktime_t h_lock_acquire_time;
static struct completion l_has_lock; // 用于同步L线程获得锁

int low_prio_thread(void *data)
{
    struct sched_param sp;
    int i;
    sp.sched_priority = 10;
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);

    pr_alert("rbmutex L: START, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    
    rb_mutex_lock(&test_lock);
    pr_alert("rbmutex L: GOT rbmutex, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    
    // 通知主线程L已经获得锁
    complete(&l_has_lock);
    msleep(20);
    // 持续运行一段时间，让M线程有机会运行
    for (i = 0; i < 5000000; i++) {
        if (i % 1000000 == 0) {
            pr_alert("rbmutex L: iteration=%d, current prio: %d, normal_prio: %d\n",
                    i, current->prio, current->normal_prio);
        }
        // 做一些计算工作，避免编译器优化掉循环
        __asm__ volatile("" : : : "memory");
    }
    
    rb_mutex_unlock(&test_lock);
    pr_alert("rbmutex L: RELEASE rbmutex, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    schedule();

    for (i = 0; i < 5000000; i++) {
        if (i % 1000000 == 0) {
            pr_alert("rbmutex L: iteration=%d, current prio: %d, normal_prio: %d\n",
                    i, current->prio, current->normal_prio);
        }
        // 做一些计算工作，避免编译器优化掉循环
        __asm__ volatile("" : : : "memory");
    }
    pr_alert("rbmutex L: END, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    return 0;
}

int mid_prio_thread(void *data)
{
    struct sched_param sp;
    int i;
    sp.sched_priority = 15;
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    
    pr_alert("rbmutex M: START, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);    
    msleep(10);
    // 持续运行一段时间，不需要锁
    for (i = 0; i < 1000000000; i++) {
        if (i % 100000000 == 0) {
            pr_alert("rbmutex M: iteration=%d, prio: %d, normal_prio: %d\n", i, current->prio, current->normal_prio);
        }
        // 做一些计算工作
        __asm__ volatile("" : : : "memory");
        if(i == 500000000){
            schedule();
        }
    }
    
    pr_alert("rbmutex M: END, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    return 0;
}

int high_prio_thread(void *data)
{
    struct sched_param sp;
    sp.sched_priority = 20;
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    
    pr_alert("rbmutex H: START, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    
    spin_lock(&time_measure_lock);
    // 记录开始尝试获取锁的时间
    h_block_start_time = ktime_get();
    pr_alert("rbmutex H: Attempting to acquire lock at time: %lld ns\n", ktime_to_ns(h_block_start_time));
    if(!rb_mutex_trylock(&test_lock)){//确保能记录H第一次试图获取锁的时间
        spin_unlock(&time_measure_lock);
    }
    rb_mutex_lock(&test_lock);
    // 记录获得锁的时间
    h_lock_acquire_time = ktime_get();
    pr_alert("###\n");
    pr_alert("rbmutex H: GOT rbmutex, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    pr_alert("rbmutex H: Acquired lock at time: %lld ns\n", ktime_to_ns(h_lock_acquire_time));
    pr_alert("###\n");
    // 计算阻塞时间
    ktime_t block_duration = ktime_sub(h_lock_acquire_time, h_block_start_time);
    pr_alert("rbmutex H: Block duration: %lld ns (%lld us)\n", 
            ktime_to_ns(block_duration), ktime_to_ns(block_duration) / 1000);
    rb_mutex_unlock(&test_lock);
    pr_alert("rbmutex H: RELEASE rbmutex, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    spin_unlock(&time_measure_lock);
    
    pr_alert("rbmutex H: END, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    return 0;
}

static int rbmutex_init(void)   // 初始化rb_mutex测试
{
    pr_info("rbmutex test module loaded\n");
    struct sched_param sp;
    sp.sched_priority = 25;
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);

    rb_mutex_init(&test_lock);
    init_completion(&l_has_lock);

    // 创建低优先级线程L
    task_l = kthread_create(low_prio_thread, NULL, "low_prio_thread");
    if (IS_ERR(task_l)) {
        pr_err("Failed to create low priority thread\n");
        return PTR_ERR(task_l);
    }
    kthread_bind(task_l, 0); // 绑定到CPU 0
    wake_up_process(task_l);
    
    // 等待L线程获得锁
    wait_for_completion(&l_has_lock);
    pr_info("L thread has acquired the lock, now creating M and H threads\n");
    
    // 创建中优先级线程M
    task_m = kthread_create(mid_prio_thread, NULL, "mid_prio_thread");
    if (IS_ERR(task_m)) {
        pr_err("Failed to create mid priority thread\n");
        return PTR_ERR(task_m);
    }
    kthread_bind(task_m, 0); // 绑定到CPU 0
    wake_up_process(task_m);
    
    // 创建高优先级线程H
    task_h = kthread_create(high_prio_thread, NULL, "high_prio_thread");
    if (IS_ERR(task_h)) {
        pr_err("Failed to create high priority thread\n");
        return PTR_ERR(task_h);
    }
    kthread_bind(task_h, 0); // 绑定到CPU 0
    wake_up_process(task_h);

    return 0;
}

static void rbmutex_exit(void)  // 退出rb_mutex测试
{
    pr_info("rbmutex test module unloaded\n");
}


// ===================== rb_mutex 死锁检测测试 =====================
static struct rb_mutex mutexA, mutexB;
static struct task_struct *task1, *task2;

int thread1_fn(void *data)
{
    pr_alert("[DLTEST] T1: locking mutexA\n");
    rb_mutex_lock(&mutexA);
    pr_alert("[DLTEST] T1: locked mutexA, sleeping...\n");
    msleep(100);

    pr_alert("[DLTEST] T1: trying to lock mutexB (should deadlock)\n");
    if (rb_mutex_lock(&mutexB) == RB_MUTEX_DEADLOCK_ERR) {
        pr_alert("[DLTEST] T1: Deadlock detected by rb_mutex!\n");
    } else {
        pr_alert("[DLTEST] T1: locked mutexB (unexpected)\n");
        rb_mutex_unlock(&mutexB);
    }
    rb_mutex_unlock(&mutexA);
    return 0;
}

int thread2_fn(void *data)
{
    pr_alert("[DLTEST] T2: locking mutexB\n");
    rb_mutex_lock(&mutexB);
    pr_alert("[DLTEST] T2: locked mutexB, sleeping...\n");
    msleep(100);

    pr_alert("[DLTEST] T2: trying to lock mutexA (should deadlock)\n");
    if (rb_mutex_lock(&mutexA) == RB_MUTEX_DEADLOCK_ERR) {
        pr_alert("[DLTEST] T2: Deadlock detected by rb_mutex!\n");
    } else {
        pr_alert("[DLTEST] T2: locked mutexA (unexpected)\n");
        rb_mutex_unlock(&mutexA);
    }
    rb_mutex_unlock(&mutexB);
    return 0;
}

static int __init rb_mutex_deadlock_test_init(void)
{
    pr_info("[DLTEST] rb_mutex deadlock test module loaded\n");
    rb_mutex_init(&mutexA);
    rb_mutex_init(&mutexB);

    task1 = kthread_run(thread1_fn, NULL, "rbm_t1");
    task2 = kthread_run(thread2_fn, NULL, "rbm_t2");
    return 0;
}

static void __exit rb_mutex_deadlock_test_exit(void)
{
    pr_info("[DLTEST] rb_mutex deadlock test module unloaded\n");
    pr_info("[DLTEST] Total deadlocks detected: %d\n", rb_mutex_get_deadlock_count());
}

// 合并统一入口/出口
static int __init rb_mutex_user_init(void)
{
    int ret = 0;
    // 原有rtmutex测试
    ret = rbmutex_init();
    msleep(3000);
    // 死锁测试
    rb_mutex_deadlock_test_init();
    return ret;
}

static void __exit rb_mutex_user_exit(void)
{
    rbmutex_exit();
    rb_mutex_deadlock_test_exit();
    pr_info("[DLTEST] Total deadlocks detected: %d\n", rb_mutex_get_deadlock_count());
    pr_info("rb_mutex_user module unloaded\n");
}

module_init(rb_mutex_user_init);
module_exit(rb_mutex_user_exit);
MODULE_LICENSE("GPL"); 