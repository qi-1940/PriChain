#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/rtmutex.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/delay.h>
#include <linux/cpumask.h>
#include <linux/ktime.h>
#include <linux/completion.h>
#include <linux/rb_mutex.h>

struct sched_param {
    int sched_priority;
};

// 支持三种锁类型
static struct rb_mutex rb_lock_x, rb_lock_y;
static struct rt_mutex rt_lock_x, rt_lock_y;
static struct mutex regular_lock_x, regular_lock_y;

// 线程任务结构
static struct task_struct *taskA, *taskB, *taskC, *taskD, *taskE;

// 同步机制
static struct completion D_finished, E_finished;
static struct completion C_created, B_created, D_created  ;

// 测试模式：0=rb_mutex, 1=rtmutex, 2=mutex
static int test_mode = 0;
module_param(test_mode, int, 0644);
MODULE_PARM_DESC(test_mode, "Test mode: 0=rb_mutex, 1=rtmutex, 2=mutex");

void lock_X(void) {
    switch (test_mode) {
        case 0: rb_mutex_lock(&rb_lock_x); break;
        case 1: rt_mutex_lock(&rt_lock_x); break;
        case 2: mutex_lock(&regular_lock_x); break;
    }
}
void unlock_X(void) {
    switch (test_mode) {
        case 0: rb_mutex_unlock(&rb_lock_x); break;
        case 1: rt_mutex_unlock(&rt_lock_x); break;
        case 2: mutex_unlock(&regular_lock_x); break;
    }
}
void lock_Y(void) {
    switch (test_mode) {
        case 0: rb_mutex_lock(&rb_lock_y); break;
        case 1: rt_mutex_lock(&rt_lock_y); break;
        case 2: mutex_lock(&regular_lock_y); break;
    }
}
void unlock_Y(void) {
    switch (test_mode) {
        case 0: rb_mutex_unlock(&rb_lock_y); break;
        case 1: rt_mutex_unlock(&rt_lock_y); break;
        case 2: mutex_unlock(&regular_lock_y); break;
    }
}

// 1A线程：最低优先级，依次获得X、Y
int threadA_func(void *data) {
    struct sched_param sp;
    int i;
    sp.sched_priority = 1;
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    pr_alert("[MULTILOCK_PRIO_TEST] A: START, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);

    lock_X();
    pr_alert("[MULTILOCK_PRIO_TEST] A: GOT LOCK X, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    lock_Y();
    pr_alert("[MULTILOCK_PRIO_TEST] A: GOT LOCK Y, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    msleep(10);
    
    // 做一些工作来展示优先级继承效果
    for (i = 0; i < 1000000000; i++) {
        if (i % 200000000 == 0) {
            pr_alert("[MULTILOCK_PRIO_TEST] A: working iteration %d, prio: %d, normal_prio: %d\n", 
                     i, current->prio, current->normal_prio);
            schedule();
        }
        __asm__ volatile("" : : : "memory");
    }
    
    pr_alert("[MULTILOCK_PRIO_TEST] A: releasing Y, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    unlock_Y();
    pr_alert("[MULTILOCK_PRIO_TEST] A: RELEASED Y, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    
    // 做一些工作验证优先级继承是否正确
    for (i = 0; i < 200000000; i++) {
        if (i % 50000000 == 0) {
            pr_alert("[MULTILOCK_PRIO_TEST] A: after Y released, working iteration %d, prio: %d, normal_prio: %d\n", 
                     i, current->prio, current->normal_prio);
            schedule();
        }
        __asm__ volatile("" : : : "memory");
    }
    
    pr_alert("[MULTILOCK_PRIO_TEST] A: releasing X, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    unlock_X();
    pr_alert("[MULTILOCK_PRIO_TEST] A: RELEASED X, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    pr_alert("[MULTILOCK_PRIO_TEST] A: END, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    return 0;
}

// 2B线程：优先级2，干扰线程，不需要锁，抢占A
int threadB_func(void *data) {
    struct sched_param sp;
    int i = 0;
    sp.sched_priority = 2;
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    pr_alert("[MULTILOCK_PRIO_TEST] B: START, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    
    // 通知主线程B已经创建
    complete(&B_created);
    msleep(5);
    
    // 持续抢占A，直到D结束
    while (!kthread_should_stop()) {
        i++;
        
        if (i % 100000000 == 0) {
            pr_alert("[MULTILOCK_PRIO_TEST] B: preempting A, iteration %d, prio: %d, normal_prio: %d\n", 
                     i, current->prio, current->normal_prio);
            // 检查D是否已经结束
            if (try_wait_for_completion(&D_finished)) {
                pr_alert("[MULTILOCK_PRIO_TEST] B: D has finished, B will stop, prio: %d, normal_prio: %d\n", 
                         current->prio, current->normal_prio);
                break;
            }
            schedule();
        }
        __asm__ volatile("" : : : "memory");
    }
    
    pr_alert("[MULTILOCK_PRIO_TEST] B: END, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    return 0;
}

// 4C线程：优先级4，干扰线程，不需要锁，抢占A
int threadC_func(void *data) {
    struct sched_param sp;
    int i = 0;
    sp.sched_priority = 4;
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    // 等待B线程完全启动
    wait_for_completion(&B_created);
    pr_alert("[MULTILOCK_PRIO_TEST] C: START, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    
    // 通知D和E线程C已经创建
    complete(&C_created);
    msleep(5);
    
    // 持续抢占A，直到E结束
    while (!kthread_should_stop()) {
        i++;
        
        if (i % 100000000 == 0) {
            pr_alert("[MULTILOCK_PRIO_TEST] C: preempting A, iteration %d, prio: %d, normal_prio: %d\n", 
                     i, current->prio, current->normal_prio);
            // 检查E是否已经结束
            if (try_wait_for_completion(&E_finished)) {
                pr_alert("[MULTILOCK_PRIO_TEST] C: E has finished, C will stop, prio: %d, normal_prio: %d\n", 
                         current->prio, current->normal_prio);
                break;
            }
            schedule();
        }
        __asm__ volatile("" : : : "memory");
    }
    
    pr_alert("[MULTILOCK_PRIO_TEST] C: END, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    return 0;
}

// 3D线程：优先级3，阻塞在X，等待C生成后再运行
int threadD_func(void *data) {
    struct sched_param sp;
    sp.sched_priority = 3;
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    // 等待C线程完全启动
    wait_for_completion(&C_created);
    msleep(1);
    complete(&D_created);
    pr_alert("[MULTILOCK_PRIO_TEST] D: START, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    pr_alert("[MULTILOCK_PRIO_TEST] D: TRY LOCK X, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    lock_X();
    pr_alert("[MULTILOCK_PRIO_TEST] D: GOT LOCK X, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    unlock_X();
    pr_alert("[MULTILOCK_PRIO_TEST] D: RELEASE LOCK X, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    pr_alert("[MULTILOCK_PRIO_TEST] D: END, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    complete(&D_finished);
    return 0;
}

// 5E线程：优先级5，阻塞在Y，等待C创建后再运行
int threadE_func(void *data) {
    struct sched_param sp;
    sp.sched_priority = 5;
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    wait_for_completion(&D_created);
    pr_alert("[MULTILOCK_PRIO_TEST] E: START, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    pr_alert("[MULTILOCK_PRIO_TEST] E: TRY LOCK Y, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    lock_Y();
    pr_alert("[MULTILOCK_PRIO_TEST] E: GOT LOCK Y, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    unlock_Y();
    pr_alert("[MULTILOCK_PRIO_TEST] E: RELEASE LOCK Y, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    pr_alert("[MULTILOCK_PRIO_TEST] E: END, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    complete(&E_finished);
    return 0;
}

static int __init multilock_prio_test_init(void) {
    char *test_names[] = {"rb_mutex", "rtmutex", "mutex"};
    struct sched_param sp;
    
    // 检查test_mode参数有效性
    if (test_mode < 0 || test_mode > 2) {
        pr_err("[MULTILOCK_PRIO_TEST] Invalid test_mode %d, must be 0-2\n", test_mode);
        return -EINVAL;
    }
    
    pr_info("[MULTILOCK_PRIO_TEST] rb_mutex multi-lock priority test module loaded\n");
    pr_info("[MULTILOCK_PRIO_TEST] Testing with: %s\n", test_names[test_mode]);
    
    // 设置主线程高优先级，仿照highest测试
    sp.sched_priority = 90;
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);

    rb_mutex_init(&rb_lock_x); 
    rb_mutex_init(&rb_lock_y);
    rt_mutex_init(&rt_lock_x);
     rt_mutex_init(&rt_lock_y);
    mutex_init(&regular_lock_x); 
    mutex_init(&regular_lock_y);

    init_completion(&D_finished);
     init_completion(&E_finished);
    init_completion(&C_created); 
    init_completion(&B_created); 
    init_completion(&D_created);
    // 创建线程A
    taskA = kthread_create(threadA_func, NULL, "thread_A");
    kthread_bind(taskA, 0); 
    wake_up_process(taskA);
    // 创建线程B
    taskB = kthread_create(threadB_func, NULL, "thread_B");
    kthread_bind(taskB, 0); 
    wake_up_process(taskB);
    // 创建线程C
    taskC = kthread_create(threadC_func, NULL, "thread_C");
    kthread_bind(taskC, 0); 
    wake_up_process(taskC);
    // 创建线程D（绑定到CPU1）
    taskD = kthread_create(threadD_func, NULL, "thread_D");
    kthread_bind(taskD, 1); 
    wake_up_process(taskD);
    // 创建线程E（绑定到CPU1）
    taskE = kthread_create(threadE_func, NULL, "thread_E");
    kthread_bind(taskE, 1); 
    wake_up_process(taskE);
    return 0;
}

static void __exit multilock_prio_test_exit(void) {
    pr_info("[MULTILOCK_PRIO_TEST] rb_mutex multi-lock priority test module unloaded\n");
}

module_init(multilock_prio_test_init);
module_exit(multilock_prio_test_exit);
MODULE_LICENSE("GPL");