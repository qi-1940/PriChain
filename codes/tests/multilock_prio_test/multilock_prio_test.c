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
static struct completion A_has_X, A_has_Y;
static struct completion B_blocked, C_blocked, D_blocked, E_blocked;
static struct completion C_sleep_done;

// 结果记录
static int completion_order[5];
static int completion_index = 0;
static DEFINE_SPINLOCK(result_lock);

// 测试模式：0=rb_mutex, 1=rtmutex, 2=mutex
static int test_mode = 0;
module_param(test_mode, int, 0644);
MODULE_PARM_DESC(test_mode, "Test mode: 0=rb_mutex, 1=rtmutex, 2=mutex");

void record_completion(int thread_id) {
    spin_lock(&result_lock);
    if (completion_index < 5) {
        completion_order[completion_index++] = thread_id;
    }
    spin_unlock(&result_lock);
}

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
    sp.sched_priority = 1;
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    pr_alert("[MULTILOCK_PRIO_TEST] A: START, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    lock_X();
    pr_alert("[MULTILOCK_PRIO_TEST] A: GOT X\n");
    complete(&A_has_X);
    msleep(10);
    lock_Y();
    pr_alert("[MULTILOCK_PRIO_TEST] A: GOT Y\n");
    complete(&A_has_Y);
    // 等待E被阻塞，说明所有线程都已阻塞在A身上
    wait_for_completion(&E_blocked);
    pr_alert("[MULTILOCK_PRIO_TEST] A: All threads blocked, releasing Y\n");
    unlock_Y();
    pr_alert("[MULTILOCK_PRIO_TEST] A: RELEASED Y\n");
    msleep(50);
    pr_alert("[MULTILOCK_PRIO_TEST] A: releasing X\n");
    unlock_X();
    pr_alert("[MULTILOCK_PRIO_TEST] A: RELEASED X\n");
    record_completion(1);
    pr_alert("[MULTILOCK_PRIO_TEST] A: END\n");
    return 0;
}

// 2B线程：优先级2，阻塞在Y
int threadB_func(void *data) {
    struct sched_param sp;
    sp.sched_priority = 2;
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    pr_alert("[MULTILOCK_PRIO_TEST] B: START\n");
    wait_for_completion(&A_has_Y);
    pr_alert("[MULTILOCK_PRIO_TEST] B: Trying to get Y\n");
    complete(&B_blocked);
    lock_Y();
    pr_alert("[MULTILOCK_PRIO_TEST] B: GOT Y\n");
    unlock_Y();
    pr_alert("[MULTILOCK_PRIO_TEST] B: RELEASED Y\n");
    record_completion(2);
    pr_alert("[MULTILOCK_PRIO_TEST] B: END\n");
    return 0;
}

// 4C线程：优先级4，阻塞在Y，先睡眠
int threadC_func(void *data) {
    struct sched_param sp;
    sp.sched_priority = 4;
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    pr_alert("[MULTILOCK_PRIO_TEST] C: START\n");
    wait_for_completion(&A_has_Y);
    pr_alert("[MULTILOCK_PRIO_TEST] C: Sleeping...\n");
    msleep(100);
    complete(&C_sleep_done);
    pr_alert("[MULTILOCK_PRIO_TEST] C: Trying to get Y\n");
    complete(&C_blocked);
    lock_Y();
    pr_alert("[MULTILOCK_PRIO_TEST] C: GOT Y\n");
    unlock_Y();
    pr_alert("[MULTILOCK_PRIO_TEST] C: RELEASED Y\n");
    record_completion(4);
    pr_alert("[MULTILOCK_PRIO_TEST] C: END\n");
    return 0;
}

// 3D线程：优先级3，阻塞在X，等待C睡眠后再运行
int threadD_func(void *data) {
    struct sched_param sp;
    sp.sched_priority = 3;
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    pr_alert("[MULTILOCK_PRIO_TEST] D: START\n");
    wait_for_completion(&C_sleep_done);
    pr_alert("[MULTILOCK_PRIO_TEST] D: Trying to get X\n");
    complete(&D_blocked);
    lock_X();
    pr_alert("[MULTILOCK_PRIO_TEST] D: GOT X\n");
    unlock_X();
    pr_alert("[MULTILOCK_PRIO_TEST] D: RELEASED X\n");
    record_completion(3);
    pr_alert("[MULTILOCK_PRIO_TEST] D: END\n");
    return 0;
}

// 5E线程：优先级5，阻塞在Y，等待C和D都阻塞后再运行
int threadE_func(void *data) {
    struct sched_param sp;
    sp.sched_priority = 5;
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    pr_alert("[MULTILOCK_PRIO_TEST] E: START\n");
    wait_for_completion(&C_blocked);
    wait_for_completion(&D_blocked);
    pr_alert("[MULTILOCK_PRIO_TEST] E: Trying to get Y\n");
    complete(&E_blocked);
    lock_Y();
    pr_alert("[MULTILOCK_PRIO_TEST] E: GOT Y\n");
    unlock_Y();
    pr_alert("[MULTILOCK_PRIO_TEST] E: RELEASED Y\n");
    record_completion(5);
    pr_alert("[MULTILOCK_PRIO_TEST] E: END\n");
    return 0;
}

void print_test_results(void) {
    int i;
    char *test_names[] = {"rb_mutex", "rtmutex", "mutex"};
    pr_alert("[MULTILOCK_PRIO_TEST] ========== TEST RESULTS ==========\n");
    pr_alert("[MULTILOCK_PRIO_TEST] Test mode: %s\n", test_names[test_mode]);
    pr_alert("[MULTILOCK_PRIO_TEST] Expected: E(5), C(4), D(3), A(1), B(2)\n");
    pr_alert("[MULTILOCK_PRIO_TEST] Actual: ");
    spin_lock(&result_lock);
    for (i = 0; i < completion_index; i++) {
        if (completion_order[i] == 5) pr_cont("E(5) ");
        else if (completion_order[i] == 4) pr_cont("C(4) ");
        else if (completion_order[i] == 3) pr_cont("D(3) ");
        else if (completion_order[i] == 2) pr_cont("B(2) ");
        else if (completion_order[i] == 1) pr_cont("A(1) ");
    }
    pr_cont("\n");
    // 判断测试结果
    if (completion_index >= 5 &&
        completion_order[0] == 5 &&
        completion_order[1] == 4 &&
        completion_order[2] == 3 &&
        completion_order[3] == 1 &&
        completion_order[4] == 2) {
        pr_alert("[MULTILOCK_PRIO_TEST] RESULT: PASS - Multi-lock priority inheritance correct!\n");
    } else {
        pr_alert("[MULTILOCK_PRIO_TEST] RESULT: FAIL - Multi-lock priority inheritance error!\n");
    }
    spin_unlock(&result_lock);
    pr_alert("[MULTILOCK_PRIO_TEST] =====================================\n");
}

static int __init multilock_prio_test_init(void) {
    pr_info("[MULTILOCK_PRIO_TEST] rb_mutex multi-lock priority test module loaded\n");
    char *test_names[] = {"rb_mutex", "rtmutex", "mutex"};
    pr_info("[MULTILOCK_PRIO_TEST] Testing with: %s\n", test_names[test_mode]);
    rb_mutex_init(&rb_lock_x); rb_mutex_init(&rb_lock_y);
    rt_mutex_init(&rt_lock_x); rt_mutex_init(&rt_lock_y);
    mutex_init(&regular_lock_x); mutex_init(&regular_lock_y);
    init_completion(&A_has_X); init_completion(&A_has_Y);
    init_completion(&B_blocked); init_completion(&C_blocked);
    init_completion(&D_blocked); init_completion(&E_blocked);
    init_completion(&C_sleep_done);
    // 创建线程A
    taskA = kthread_create(threadA_func, NULL, "thread_A");
    if (IS_ERR(taskA)) { pr_err("[MULTILOCK_PRIO_TEST] Failed to create thread A\n"); return PTR_ERR(taskA); }
    kthread_bind(taskA, 0); wake_up_process(taskA);
    // 创建线程B
    taskB = kthread_create(threadB_func, NULL, "thread_B");
    if (IS_ERR(taskB)) { pr_err("[MULTILOCK_PRIO_TEST] Failed to create thread B\n"); return PTR_ERR(taskB); }
    kthread_bind(taskB, 0); wake_up_process(taskB);
    // 创建线程C
    taskC = kthread_create(threadC_func, NULL, "thread_C");
    if (IS_ERR(taskC)) { pr_err("[MULTILOCK_PRIO_TEST] Failed to create thread C\n"); return PTR_ERR(taskC); }
    kthread_bind(taskC, 0); wake_up_process(taskC);
    // 创建线程D
    taskD = kthread_create(threadD_func, NULL, "thread_D");
    if (IS_ERR(taskD)) { pr_err("[MULTILOCK_PRIO_TEST] Failed to create thread D\n"); return PTR_ERR(taskD); }
    kthread_bind(taskD, 0); wake_up_process(taskD);
    // 创建线程E
    taskE = kthread_create(threadE_func, NULL, "thread_E");
    if (IS_ERR(taskE)) { pr_err("[MULTILOCK_PRIO_TEST] Failed to create thread E\n"); return PTR_ERR(taskE); }
    kthread_bind(taskE, 0); wake_up_process(taskE);
    msleep(4000);
    print_test_results();
    return 0;
}

static void __exit multilock_prio_test_exit(void) {
    pr_info("[MULTILOCK_PRIO_TEST] rb_mutex multi-lock priority test module unloaded\n");
}

module_init(multilock_prio_test_init);
module_exit(multilock_prio_test_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("rb_mutex multi-lock priority inheritance test");
MODULE_AUTHOR("PriChain"); 