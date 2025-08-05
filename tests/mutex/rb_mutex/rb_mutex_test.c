#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/rtmutex.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/delay.h>
#include <linux/completion.h>
#include <linux/rb_mutex.h>

struct sched_param {
    int sched_priority;
};

// 锁声明
static struct rt_mutex rt_lock_l1;
static struct rt_mutex rt_lock_l2;
static struct rt_mutex rt_lock_l3;
static struct rt_mutex rt_lock_l4;
static struct rt_mutex rt_lock_l5;

// 同步机制
static struct completion p2_finished;
static struct completion p6_finished;

// 线程声明
static struct task_struct *p0;
static struct task_struct *p1;
static struct task_struct *p2;
static struct task_struct *p3;
static struct task_struct *p4;
static struct task_struct *p5;
static struct task_struct *p6;
static struct task_struct *p7;
static struct task_struct *p8;
static struct task_struct *p9;
static struct task_struct *p10;

// 封装线程创建函数
static struct task_struct* create_and_start_thread(int (*p)(void *data),int which_cpu) {
    struct task_struct *task;
    task = kthread_create(p,NULL,NULL);
    kthread_bind(task, which_cpu);
    wake_up_process(task);
    return task;
}

void lock_mutex(int test_mode) {
    switch (test_mode) {
        case 0: rb_mutex_lock(&rb_test_lock); break;
        case 1: rt_mutex_lock(&rt_test_lock); break;
        case 2: mutex_lock(&regular_test_lock); break;
    }
}

void unlock_mutex(int test_mode) {
    switch (test_mode) {
        case 0: rb_mutex_unlock(&rb_test_lock); break;
        case 1: rt_mutex_unlock(&rt_test_lock); break;
        case 2: mutex_unlock(&regular_test_lock); break;
    }
}

// 线程函数定义
static int thread_p0_func(void *data) {
    struct sched_param sp = { .sched_priority = 0 };
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    while (!kthread_should_stop()) {
        for (int i = 0; i < 1000000; i++) {
            __asm__ volatile("" : : : "memory");
        }
        schedule();
    }
    return 0;
}

static int thread_p1_func(void *data) {
    struct sched_param sp = { .sched_priority = 10 };
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    while (!kthread_should_stop()) {
        for (int i = 0; i < 1000000; i++) {
            __asm__ volatile("" : : : "memory");
        }
        schedule();
    }
    return 0;
}

static int thread_p2_func(void *data) {
    struct sched_param sp = { .sched_priority = 15 };
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    while (!kthread_should_stop()) {
        for (int i = 0; i < 1000000; i++) {
            __asm__ volatile("" : : : "memory");
        }
        schedule();
    }
    return 0;
}

static int thread_p3_func(void *data) {
    struct sched_param sp = { .sched_priority = 20 };
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    while (!kthread_should_stop()) {
        for (int i = 0; i < 1000000; i++) {
            __asm__ volatile("" : : : "memory");
        }
        schedule();
    }
    return 0;
}

static int thread_p4_func(void *data) {
    struct sched_param sp = { .sched_priority = 5 };
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    while (!kthread_should_stop()) {
        for (int i = 0; i < 1000000; i++) {
            __asm__ volatile("" : : : "memory");
        }
        schedule();
    }
    return 0;
}

static int thread_p5_func(void *data) {
    struct sched_param sp = { .sched_priority = 15 };
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    while (!kthread_should_stop()) {
        for (int i = 0; i < 1000000; i++) {
            __asm__ volatile("" : : : "memory");
        }
        schedule();
    }
    return 0;
}

static int thread_p6_func(void *data) {
    struct sched_param sp = { .sched_priority = 25 };
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    while (!kthread_should_stop()) {
        for (int i = 0; i < 1000000; i++) {
            __asm__ volatile("" : : : "memory");
        }
        schedule();
    }
    return 0;
}

static int thread_p7_func(void *data) {
    struct sched_param sp = { .sched_priority = 50 };
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    while (!kthread_should_stop()) {
        for (int i = 0; i < 1000000; i++) {
            __asm__ volatile("" : : : "memory");
        }
        schedule();
    }
    return 0;
}

static int thread_p8_func(void *data) {
    struct sched_param sp = { .sched_priority = 35 };
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    while (!kthread_should_stop()) {
        for (int i = 0; i < 1000000; i++) {
            __asm__ volatile("" : : : "memory");
        }
        schedule();
    }
    return 0;
}

static int thread_p9_func(void *data) {
    struct sched_param sp = { .sched_priority = 45 };
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    while (!kthread_should_stop()) {
        for (int i = 0; i < 1000000; i++) {
            __asm__ volatile("" : : : "memory");
        }
        schedule();
    }
    return 0;
}

static int thread_p10_func(void *data) {
    struct sched_param sp = { .sched_priority = 55 };
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    while (!kthread_should_stop()) {
        for (int i = 0; i < 1000000; i++) {
            __asm__ volatile("" : : : "memory");
        }
        schedule();
    }
    return 0;
}

static int __init test_init(void) {

    struct sched_param sp={
        .sched_priority = 60,
    };
    sched_setscheduler(current, SCHED_FIFO, &sp);

    // 初始化 rb_mutex 锁
    rb_mutex_init(&rb_lock_l1);
    rb_mutex_init(&rb_lock_l2);
    rb_mutex_init(&rb_lock_l3);
    rb_mutex_init(&rb_lock_l4);
    rb_mutex_init(&rb_lock_l5);
    
    // 初始化 rt_mutex 锁
    rt_mutex_init(&rt_lock_l1);
    rt_mutex_init(&rt_lock_l2);
    rt_mutex_init(&rt_lock_l3);
    rt_mutex_init(&rt_lock_l4);
    rt_mutex_init(&rt_lock_l5);

    // 初始化 mutex 锁
    mutex_init(&mu_lock_l1);
    mutex_init(&mu_lock_l2);
    mutex_init(&mu_lock_l3);
    mutex_init(&mu_lock_l4);
    mutex_init(&mu_lock_l5);
    
    pr_info("mutex_test%\n");

    pr_info("mutex%\n");
    pr_info("mutex_end%\n");
    
    pr_info("rtmutex%\n");
    create_and_start_thread(p0,0);
    pr_info("rtmutex_end%\n");
    
    pr_info("rb_mutex%\n");
    pr_info("rb_mutex_end%\n");
    
    pr_info("mutex_test_end%\n");
    return 0;
}

static void __exit test_exit(void) {
    // 停止所有线程
    if (p0) kthread_stop(p0);
    if (p1) kthread_stop(p1);
    if (p2) kthread_stop(p2);
    if (p3) kthread_stop(p3);
    if (p4) kthread_stop(p4);
    if (p5) kthread_stop(p5);
    if (p6) kthread_stop(p6);
    if (p7) kthread_stop(p7);
    if (p8) kthread_stop(p8);
    if (p9) kthread_stop(p9);
    if (p10) kthread_stop(p10);
}

module_init(test_init);
module_exit(test_exit);
MODULE_LICENSE("GPL");