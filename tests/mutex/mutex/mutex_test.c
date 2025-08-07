#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/completion.h>
#include <linux/cpumask.h>

#include <linux/mutex.h>

struct sched_param {
    int sched_priority;
};

// 锁声明
static struct mutex lock_l1;

// 同步机制
static struct completion p0_get_l1;
static struct completion p2_finished;

// 线程声明
static struct task_struct *p0;
static struct task_struct *p1;
static struct task_struct *p2;

void keep_some_time(void){
    int i,a;
    for (i = 0; i < 50000000; i++) {
        a=i+i;
        __asm__ volatile("" : : : "memory");
    }
}

// 线程创建函数
static struct task_struct* create_and_start_thread(int (*p)(void *data),int which_cpu,int count) {
    struct task_struct *task;
    task = kthread_create(p, NULL, "p%d",count);
    kthread_bind(task, which_cpu);
    wake_up_process(task);
    return task;
}

// 线程函数定义
static int thread_p0_func(void *data) {
    struct sched_param sp = { .sched_priority = 0 };
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);

    pr_info("p0_begin\n");
    mutex_lock(&lock_l1);
    pr_info("p0_l1\n");
    complete(&p0_get_l1);

    schedule();
    
    keep_some_time();
    schedule();
    mutex_unlock(&lock_l1);
    schedule();
    pr_info("p0_end\n");
    return 0;
}

static int thread_p1_func(void *data) {
    int count=0;
    struct sched_param sp = { .sched_priority = 10 };
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);

    pr_info("p1_begin\n");
    schedule();
    while (count < 100) {
        keep_some_time();
        schedule();
        count++;
    }
    pr_info("p1_end\n");
    return 0;
}

static int thread_p2_func(void *data) {
    struct sched_param sp = { .sched_priority = 15 };
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);

    pr_info("p2_begin\n");
    pr_info("p2_try\n");
    mutex_lock(&lock_l1);
    pr_info("p2_l1\n");
    keep_some_time();
    mutex_unlock(&lock_l1);
    pr_info("p2_end\n");
    complete(&p2_finished);
    return 0;
}

static int __init test_init(void) {
    struct cpumask cpu_mask;
    struct sched_param sp = { .sched_priority = 60 };
    sched_setscheduler(current, SCHED_FIFO, &sp);

    // 绑定主线程到CPU 0
    cpumask_clear(&cpu_mask);
    cpumask_set_cpu(0, &cpu_mask);
    set_cpus_allowed_ptr(current, &cpu_mask);

    pr_info("mutex\n");

    mutex_init(&lock_l1);
    init_completion(&p0_get_l1);
    init_completion(&p2_finished);
    
    p0 = create_and_start_thread(thread_p0_func,0,0);
    wait_for_completion(&p0_get_l1);
    p1 = create_and_start_thread(thread_p1_func,0,1);
    p2 = create_and_start_thread(thread_p2_func,1,2);
    wait_for_completion(&p2_finished);
    return 0;
}

static void __exit test_exit(void) {
    pr_info("mutex_end\n");
}

module_init(test_init);
module_exit(test_exit);
MODULE_LICENSE("GPL");