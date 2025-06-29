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
static struct completion A_finished;        // A结束的信号
static struct completion E_finished;        // E结束的信号
//static struct completion E_begin;           // E开始的信号

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

// 封装线程创建函数（绑定到CPU1）
static struct task_struct* create_and_start_thread_cpu1(int (*threadfn)(void *data), 
                                                       const char *name, 
                                                       void *data) {
    struct task_struct *task;
    
    task = kthread_create(threadfn, data, name);
    if (IS_ERR(task)) {
        pr_err("[HIGHEST_PRIO_TEST] Failed to create thread %s\n", name);
        return task;
    }
    
    kthread_bind(task, 1);  // 绑定到CPU 1
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
    pr_alert("[HIGHEST_PRIO_TEST] A: GOT LOCK, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    complete(&A_has_lock);
    schedule();
    //msleep(20);
    // 在临界区做适量工作，确保其他线程有时间启动
    for (i = 0; i < 500000000; i++) {
        if (i % 25000000 == 0) {
            pr_alert("[HIGHEST_PRIO_TEST] A: working iteration %d, prio: %d, normal_prio: %d\n", 
                     i, current->prio, current->normal_prio);
            // 偶尔让出CPU，给其他线程机会
            if(i / 25000000 == 3)msleep(10);
            schedule();
        }
        // 防止编译器优化掉循环
        __asm__ volatile("" : : : "memory");
    }
    
    unlock_mutex();
    schedule();
    pr_alert("[HIGHEST_PRIO_TEST] A: RELEASE LOCK, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);

    pr_alert("[HIGHEST_PRIO_TEST] A: END, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    
    // 通知B线程A已经结束
    //complete(&A_finished);
    
    return 0;
}

// 4B线程：高优先级干扰线程，不需要锁，持续抢占A
static int threadB_func(void *data) {
    int i;
    struct sched_param sp = { .sched_priority = 40 };
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    pr_alert("[HIGHEST_PRIO_TEST] B: START, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    
    // B线程持续运行，抢占A，直到E结束
    i = 0;
    while (!kthread_should_stop()) {
        if (i % 200000000 == 0) {
            pr_alert("[HIGHEST_PRIO_TEST] B: preempting A, iteration %d, prio: %d, normal_prio: %d\n", 
                     i, current->prio, current->normal_prio);
            // 偶尔让出CPU，给调度器机会检查是否有更高优先级线程
            schedule(); 
        }
        // 防止编译器优化掉循环
        __asm__ volatile("" : : : "memory");
        i++;
        
        // 周期性检查E是否已经结束
        if (i % 100000000 == 0) {
            // 检查E是否已经结束
            if (try_wait_for_completion(&E_finished)) {
                pr_alert("[HIGHEST_PRIO_TEST] B: E has finished, B will stop, prio: %d, normal_prio: %d\n", 
                         current->prio, current->normal_prio);
                break;
            }
            // 无条件让出CPU，确保A能及时通过优先级继承抢占B
            schedule();
        }
    }
    
    pr_alert("[HIGHEST_PRIO_TEST] B: END, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    return 0;
}

// 2C线程：绑定CPU1，尝试获取锁被阻塞，触发A优先级继承到20
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

// 3D线程：绑定CPU1，尝试获取锁被阻塞，触发A优先级继承到30
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
    pr_alert("[HIGHEST_PRIO_TEST] E: TRY LOCK, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    lock_mutex();
    pr_alert("[HIGHEST_PRIO_TEST] E: GOT LOCK, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    unlock_mutex();
    pr_alert("[HIGHEST_PRIO_TEST] E: RELEASE LOCK, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    pr_alert("[HIGHEST_PRIO_TEST] E: END, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    complete(&E_finished);
    return 0;
}

static int __init highest_prio_test_init(void) {
    char *test_names[] = {"rb_mutex", "rtmutex", "mutex"};
    struct sched_param sp;
    pr_info("[HIGHEST_PRIO_TEST] Highest priority test module loaded\n");
    pr_info("[HIGHEST_PRIO_TEST] Testing with: %s\n", test_names[test_mode]);
    
    // 设置主线程高优先级，仿照原rb_mutex_test.c
    sp.sched_priority = 90;
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    
    // 初始化锁
    rb_mutex_init(&rb_test_lock);
    rt_mutex_init(&rt_test_lock);
    mutex_init(&regular_test_lock);

    // 初始化同步机制
    init_completion(&A_has_lock);
    init_completion(&A_finished);
    init_completion(&E_finished);
    //init_completion(&E_begin);      
    taskA = create_and_start_thread(threadA_func, "thread_A", NULL);
    
    // 等待A线程获得锁
    wait_for_completion(&A_has_lock);
    pr_info("[HIGHEST_PRIO_TEST] A thread has acquired the lock\n");
    
    // 创建B线程，开始抢占A
    taskB = create_and_start_thread(threadB_func, "thread_B", NULL);
    
    // 等待足够时间确保B真正开始抢占A
    msleep(100);  // 增加等待时间，确保B真正抢占A
    
    // 创建C、D线程，绑定到CPU1，它们会被锁阻塞并触发优先级继承  
    taskC = create_and_start_thread_cpu1(threadC_func, "thread_C", NULL);
    msleep(10);
    taskD = create_and_start_thread_cpu1(threadD_func, "thread_D", NULL);
    
    // 等待一小段时间让优先级继承生效
    msleep(50);
    
    taskE = create_and_start_thread_cpu1(threadE_func, "thread_E", NULL);
    
    return 0;
}

static void __exit highest_prio_test_exit(void) {
    pr_info("[HIGHEST_PRIO_TEST] module unloaded\n");
}

module_init(highest_prio_test_init);
module_exit(highest_prio_test_exit);
MODULE_LICENSE("GPL");