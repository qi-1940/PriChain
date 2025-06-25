#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include "rb_mutex.h"
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/delay.h>
#include <linux/cpumask.h>
#include <linux/ktime.h>
#include <linux/completion.h>

struct sched_param {
    int sched_priority;
};

static struct rb_mutex test_lock;
static struct task_struct *task_l, *task_h, *task_m;
static ktime_t h_block_start_time;
static ktime_t h_lock_acquire_time;
static struct completion l_has_lock; // 用于同步L线程获得锁

// 正确声明（需与 rb_mutex.c 中的定义完全一致）
extern void rb_mutex_init(struct rb_mutex *lock);
extern void rb_mutex_lock(struct rb_mutex *lock);
extern void rb_mutex_unlock(struct rb_mutex *lock);
int low_prio_thread(void *data)
{
    struct sched_param sp;
    int i;
    sp.sched_priority = 10;
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);

    pr_alert("rb_mutex L: START, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    
    rb_mutex_lock(&test_lock);
    pr_alert("rb_mutex L: GOT rb_mutex, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    
    // 通知主线程L已经获得锁
    complete(&l_has_lock);
    msleep(20);
    // 持续运行一段时间，让M线程有机会运行
    for (i = 0; i < 5000000; i++) {
        if (i % 1000000 == 0) {
            pr_alert("rb_mutex L: iteration=%d, current prio: %d, normal_prio: %d\n",
                    i, current->prio, current->normal_prio);
        }
        // 做一些计算工作，避免编译器优化掉循环
        __asm__ volatile("" : : : "memory");
    }
    
    rb_mutex_unlock(&test_lock);


    for (i = 0; i < 5000000; i++) {
        if (i % 1000000 == 0) {
            pr_alert("rb_mutex L: iteration=%d, current prio: %d, normal_prio: %d\n",
                    i, current->prio, current->normal_prio);
        }
        // 做一些计算工作，避免编译器优化掉循环
        __asm__ volatile("" : : : "memory");
    }
    pr_alert("rb_mutex L: END, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    return 0;
}

int mid_prio_thread(void *data)
{
    struct sched_param sp;
    int i;
    sp.sched_priority = 15;
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    
    pr_alert("rb_mutex M: START, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);    
    msleep(10);
    // 持续运行一段时间，不需要锁
    for (i = 0; i < 1000000000; i++) {
        if (i % 100000000 == 0) {
            pr_alert("rb_mutex M: iteration=%d, prio: %d, normal_prio: %d\n", i, current->prio, current->normal_prio);
        }
        // 做一些计算工作
        __asm__ volatile("" : : : "memory");
        if(i == 9999999){
            schedule();
        }
    }
    
    pr_alert("rb_mutex M: END, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    return 0;
}

int high_prio_thread(void *data)
{
    struct sched_param sp;
    sp.sched_priority = 20;
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    
    pr_alert("rb_mutex H: START, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    
    // 记录开始尝试获取锁的时间
    h_block_start_time = ktime_get();
    pr_alert("rb_mutex H: Attempting to acquire lock at time: %lld ns\n", ktime_to_ns(h_block_start_time));
    
    rb_mutex_lock(&test_lock);
    
    // 记录获得锁的时间
    h_lock_acquire_time = ktime_get();
    pr_alert("rb_mutex H: GOT rb_mutex, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    pr_alert("rb_mutex H: Acquired lock at time: %lld ns\n", ktime_to_ns(h_lock_acquire_time));
    
    // 计算阻塞时间
    ktime_t block_duration = ktime_sub(h_lock_acquire_time, h_block_start_time);
    pr_alert("rb_mutex H: Block duration: %lld ns (%lld us)\n", 
             ktime_to_ns(block_duration), ktime_to_ns(block_duration) / 1000);
    
    rb_mutex_unlock(&test_lock);
    pr_alert("rb_mutex H: END, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    return 0;
}

static int __init rb_mutex_test_init(void)
{
    pr_info("rb_mutex test module loaded\n");
    struct sched_param sp;
    sp.sched_priority = 80;
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

static void __exit rb_mutex_test_exit(void)
{
    pr_info("rb_mutex test module unloaded\n");
}

module_init(rb_mutex_test_init);
module_exit(rb_mutex_test_exit);
MODULE_LICENSE("GPL"); 