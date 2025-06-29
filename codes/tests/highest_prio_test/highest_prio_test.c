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

// 测试用的锁（支持三种类型：rb_mutex, rtmutex, mutex）
static struct rb_mutex rb_test_lock;
static struct rt_mutex rt_test_lock;
static struct mutex regular_test_lock;

// 线程任务结构
static struct task_struct *taskA, *taskB, *taskC, *taskD, *taskE;

// 同步机制
static struct completion A_has_lock;        // A获得锁的信号
static struct completion B_slept_begin;     // B睡眠开始的信号
static struct completion B_slept_end;       // B睡眠结束的信号
static struct completion E_begin;       // E开始抢占A的信号

// 测试模式：0=rb_mutex, 1=rtmutex, 2=mutex
static int test_mode = 0;
module_param(test_mode, int, 0644);
MODULE_PARM_DESC(test_mode, "Test mode: 0=rb_mutex, 1=rtmutex, 2=mutex");

void lock_mutex(void) {
    switch (test_mode) {
        case 0: rb_mutex_lock(&rb_test_lock); break;
        case 1: rt_mutex_lock(&rt_test_lock); break;
        case 2: mutex_lock(&regular_test_lock); break;
    }
}

void unlock_mutex(void) {
    switch (test_mode) {
        case 0: rb_mutex_unlock(&rb_test_lock); break;
        case 1: rt_mutex_unlock(&rt_test_lock); break;
        case 2: mutex_unlock(&regular_test_lock); break;
    }
}

// 封装线程创建函数
static struct task_struct* create_and_start_thread(int (*threadfn)(void *data), 
                                                  const char *name, 
                                                  void *data) {
    struct task_struct *task;
    
    task = kthread_create(threadfn, data, name);
    if (IS_ERR(task)) {
        pr_err("[HIGHEST_PRIO_TEST] Failed to create thread %s\n", name);
        return task;
    }
    
    kthread_bind(task, 0);  // 绑定到CPU 0
    wake_up_process(task);
    
    return task;
}

// 1A线程：最低优先级，先获得锁，在临界区做长时间工作
static int threadA_func(void *data) {
    int i;
    struct sched_param sp = { .sched_priority = 10 };
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    pr_alert("[HIGHEST_PRIO_TEST] A: START, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);

    lock_mutex();
    complete(&A_has_lock);
    pr_alert("[HIGHEST_PRIO_TEST] A: GOT LOCK, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    //msleep(20);
    // 在临界区做长时间工作，让其他线程有机会启动和抢占
    for (i = 0; i < 1000000000; i++) {
        if (i % 100000000 == 0) {
            pr_alert("[HIGHEST_PRIO_TEST] A: working iteration %d, prio: %d, normal_prio: %d\n", 
                     i, current->prio, current->normal_prio);
            // 偶尔让出CPU，给其他线程机会
            if(i / 100000000 == 3)msleep(10);
            schedule();
        }
        // 防止编译器优化掉循环
        __asm__ volatile("" : : : "memory");
    }
    
    unlock_mutex();
    schedule();
    pr_alert("[HIGHEST_PRIO_TEST] A: RELEASE LOCK, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);

    pr_alert("[HIGHEST_PRIO_TEST] A: END, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    return 0;
}

// 4B线程：简单工作线程，不需要锁
static int threadB_func(void *data) {
    int i;
    struct sched_param sp = { .sched_priority = 40 };
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    pr_alert("[HIGHEST_PRIO_TEST] B: START, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    
    // B线程做一些不需要锁的工作
    for (i = 0; i < 50000000; i++) {
        if (i % 10000000 == 0) {
            pr_alert("[HIGHEST_PRIO_TEST] B: working iteration %d, prio: %d, normal_prio: %d\n", 
                     i, current->prio, current->normal_prio);
            //msleep(10);
            schedule(); 
        }
        // 防止编译器优化掉循环
        __asm__ volatile("" : : : "memory");
    }

    complete(&B_slept_begin);
    msleep(100);
    complete(&B_slept_end);

    wait_for_completion(&E_begin);

    for (i = 0; i < 50000000; i++) {
        if (i % 10000000 == 0) {
            pr_alert("[HIGHEST_PRIO_TEST] B: working iteration %d, prio: %d, normal_prio: %d\n", 
                     i, current->prio, current->normal_prio);
            //msleep(10);
            schedule(); 
        }
        // 防止编译器优化掉循环
        __asm__ volatile("" : : : "memory");
    }
    
    pr_alert("[HIGHEST_PRIO_TEST] B: END, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    return 0;
}

// 2C线程：尝试获取锁被阻塞
static int threadC_func(void *data) {
    struct sched_param sp = { .sched_priority = 20 };
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    pr_alert("[HIGHEST_PRIO_TEST] C: START, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    pr_alert("[HIGHEST_PRIO_TEST] C: TRY LOCK, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    lock_mutex();   
    pr_alert("[HIGHEST_PRIO_TEST] C: GOT LOCK, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    unlock_mutex();
    pr_alert("[HIGHEST_PRIO_TEST] C: RELEASE LOCK, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    pr_alert("[HIGHEST_PRIO_TEST] C: END, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    return 0;
}

// 3D线程：尝试获取锁被阻塞
static int threadD_func(void *data) {
    struct sched_param sp = { .sched_priority = 30 };
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    pr_alert("[HIGHEST_PRIO_TEST] D: START, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    pr_alert("[HIGHEST_PRIO_TEST] D: TRY LOCK, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    lock_mutex();
    pr_alert("[HIGHEST_PRIO_TEST] D: GOT LOCK, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    unlock_mutex();
    pr_alert("[HIGHEST_PRIO_TEST] D: RELEASE LOCK, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    pr_alert("[HIGHEST_PRIO_TEST] D: END, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    return 0;
}

// 5E线程：最高优先级，被阻塞，触发1A优先级继承
static int threadE_func(void *data) {
    struct sched_param sp = { .sched_priority = 50 };
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    pr_alert("[HIGHEST_PRIO_TEST] E: START, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);

    complete(&E_begin);

    pr_alert("[HIGHEST_PRIO_TEST] E: TRY LOCK, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    lock_mutex();
    pr_alert("[HIGHEST_PRIO_TEST] E: GOT LOCK, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);

    unlock_mutex();
    pr_alert("[HIGHEST_PRIO_TEST] E: RELEASE LOCK, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);

    pr_alert("[HIGHEST_PRIO_TEST] E: END, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    return 0;
}

static int __init highest_prio_test_init(void) {
    pr_info("[HIGHEST_PRIO_TEST] Highest priority test module loaded\n");
    char *test_names[] = {"rb_mutex", "rtmutex", "mutex"};
    pr_info("[HIGHEST_PRIO_TEST] Testing with: %s\n", test_names[test_mode]);
    
    // 设置主线程高优先级，仿照原rb_mutex_test.c
    struct sched_param sp;
    sp.sched_priority = 90;
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    
    // 初始化锁
    rb_mutex_init(&rb_test_lock);
    rt_mutex_init(&rt_test_lock);
    mutex_init(&regular_test_lock);

    // 初始化同步机制
    init_completion(&A_has_lock);
    init_completion(&B_slept_begin);
    init_completion(&B_slept_end);
    init_completion(&E_begin);

    // 步骤1：创建线程A（优先级10，最低）
    taskA = create_and_start_thread(threadA_func, "thread_A", NULL);
    if (IS_ERR(taskA)) {
        return PTR_ERR(taskA);
    }
    
    // 等待A线程获得锁
    wait_for_completion(&A_has_lock);
    pr_info("[HIGHEST_PRIO_TEST] A thread has acquired the lock, quickly creating all threads\n");
    
    // 步骤2：立即创建线程B（优先级40）
    taskB = create_and_start_thread(threadB_func, "thread_B", NULL);
    if (IS_ERR(taskB)) {
        return PTR_ERR(taskB);
    }
    
    // 等待B线程睡眠开始
    wait_for_completion(&B_slept_begin);
    pr_info("[HIGHEST_PRIO_TEST] B thread has started sleeping\n");

    taskC = create_and_start_thread(threadC_func, "thread_C", NULL);
    if (IS_ERR(taskC)) {
        return PTR_ERR(taskC);
    }
    
    taskD = create_and_start_thread(threadD_func, "thread_D", NULL);
    if (IS_ERR(taskD)) {
        return PTR_ERR(taskD);
    }

    // 等待B线程睡眠结束
    wait_for_completion(&B_slept_end);
    pr_info("[HIGHEST_PRIO_TEST] B thread has finished sleeping\n");
    
    // 步骤4：创建E 
    taskE = create_and_start_thread(threadE_func, "thread_E", NULL);
    if (IS_ERR(taskE)) {
        return PTR_ERR(taskE);
    }
    
    return 0;
}

static void __exit highest_prio_test_exit(void) {
    pr_info("[HIGHEST_PRIO_TEST] module unloaded\n");
}

module_init(highest_prio_test_init);
module_exit(highest_prio_test_exit);
MODULE_LICENSE("GPL");