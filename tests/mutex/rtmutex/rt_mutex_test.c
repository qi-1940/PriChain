#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/rtmutex.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/completion.h>

struct sched_param {
    int sched_priority;
};

// 锁声明
static struct rt_mutex rt_lock_l1;
/*
static struct rt_mutex rt_lock_l2;
static struct rt_mutex rt_lock_l3;
static struct rt_mutex rt_lock_l4;
static struct rt_mutex rt_lock_l5;
*/

// 同步机制
static struct completion p0_get_l1;
static struct completion p2_finished;
//static struct completion p6_finished;


// 线程声明
static struct task_struct *p0;
static struct task_struct *p1;
static struct task_struct *p2;
/*
static struct task_struct *p3;
static struct task_struct *p4;
static struct task_struct *p5;
static struct task_struct *p6;
static struct task_struct *p7;
static struct task_struct *p8;
static struct task_struct *p9;
static struct task_struct *p10;
*/

void keep_some_time(void){
    int i;
    for (i = 0; i < 1000000; i++) {
        __asm__ volatile("" : : : "memory");
    }
}

// 封装线程创建函数
static struct task_struct* create_and_start_thread(int (*p)(void *data),int which_cpu) {
    struct task_struct *task;
    task = kthread_create(p,NULL,NULL);
    kthread_bind(task, which_cpu);
    wake_up_process(task);
    return task;
}

// 线程函数定义
static int thread_p0_func(void *data) {
    int lock_nums=0;//累计获得锁数量
    struct sched_param sp = { .sched_priority = 0 };
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    
    while (!kthread_should_stop()) {
        if (lock_nums==0){
            pr_info("p0_begin\n");
            rt_mutex_lock(&rt_lock_l1);
            pr_info("p0_l1\n");
            lock_nums+=1;
            complete(&p0_get_l1);
            keep_some_time();
            schedule();
            rt_mutex_unlock(&rt_lock_l1);
        }
        keep_some_time();
        schedule();
    }
    pr_info("p0_end\n");
    return 0;
}

static int thread_p1_func(void *data) {
    struct sched_param sp = { .sched_priority = 10 };
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    while (!kthread_should_stop()) {
        keep_some_time();
        schedule();
    }
    pr_info("p1_end\n");
    return 0;
}

static int thread_p2_func(void *data) {
    
    struct sched_param sp = { .sched_priority = 15 };
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    
    rt_mutex_lock(&rt_lock_l1);
    pr_info("p2_l2\n");
    keep_some_time();
    schedule();
    rt_mutex_unlock(&rt_lock_l1);
    
    keep_some_time();
    schedule();
    
    pr_info("p2_end\n");
    complete(&p2_finished);
    return 0;
}
/*

static int thread_p3_func(void *data) {
    struct sched_param sp = { .sched_priority = 20 };
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    while (!kthread_should_stop()) {
        keep_some_time();
        schedule();
    }
    return 0;
}

static int thread_p4_func(void *data) {
    struct sched_param sp = { .sched_priority = 5 };
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    while (!kthread_should_stop()) {
        keep_some_time();
        schedule();
    }
    return 0;
}

static int thread_p5_func(void *data) {
    struct sched_param sp = { .sched_priority = 15 };
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    while (!kthread_should_stop()) {
        keep_some_time();
        schedule();
    }
    return 0;
}

static int thread_p6_func(void *data) {
    struct sched_param sp = { .sched_priority = 25 };
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    while (!kthread_should_stop()) {
        keep_some_time();
        schedule();
    }
    complete(&p6_finished);
    return 0;
}

static int thread_p7_func(void *data) {
    struct sched_param sp = { .sched_priority = 50 };
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    while (!kthread_should_stop()) {
        keep_some_time();
        schedule();
    }
    return 0;
}

static int thread_p8_func(void *data) {
    struct sched_param sp = { .sched_priority = 35 };
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    while (!kthread_should_stop()) {
        keep_some_time();
        schedule();
    }
    return 0;
}

static int thread_p9_func(void *data) {
    struct sched_param sp = { .sched_priority = 45 };
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    while (!kthread_should_stop()) {
        keep_some_time();
        schedule();
    }
    return 0;
}

static int thread_p10_func(void *data) {
    struct sched_param sp = { .sched_priority = 55 };
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    while (!kthread_should_stop()) {
        keep_some_time();
        schedule();
    }
    return 0;
}

*/

static int __init test_init(void) {

    struct sched_param sp={
        .sched_priority = 60,
    };
    sched_setscheduler(current, SCHED_FIFO, &sp);

    // 初始化 rt_mutex 锁
    rt_mutex_init(&rt_lock_l1);
    //rt_mutex_init(&rt_lock_l2);
    /*
    rt_mutex_init(&rt_lock_l3);
    rt_mutex_init(&rt_lock_l4);
    rt_mutex_init(&rt_lock_l5);
    */

    // 初始化同步机制
    init_completion(&p0_get_l1);
    init_completion(&p2_finished);
    //init_completion(&p6_finished);
    
    pr_info("rtmutex\n");
    p0 = create_and_start_thread(thread_p0_func,0);
    pr_info("p0_created\n");
    wait_for_completion(&p0_get_l1);
    p1 = create_and_start_thread(thread_p1_func,0);
    p2 = create_and_start_thread(thread_p2_func,0);
    pr_info("rtmutex_end\n");
    
    return 0;
}

static void __exit test_exit(void) {
    // 停止所有线程
    /*
    if (p0) kthread_stop(p0);
    if (p1) kthread_stop(p1);
    if (p2) kthread_stop(p2);
    */
    /*
    if (p3) kthread_stop(p3);
    if (p4) kthread_stop(p4);
    if (p5) kthread_stop(p5);
    if (p6) kthread_stop(p6);
    if (p7) kthread_stop(p7);
    if (p8) kthread_stop(p8);
    if (p9) kthread_stop(p9);
    if (p10) kthread_stop(p10);
    */
}

module_init(test_init);
module_exit(test_exit);
MODULE_LICENSE("GPL");