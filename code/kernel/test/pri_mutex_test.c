#include <linux/module.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/sched/types.h>
#include <linux/delay.h>
#include <linux/pri_mutex.h> // 你的pri_mutex头文件

static struct pri_mutex test_mutex;
static struct pri_mutex test_mutex2;
static struct task_struct *taskA, *taskB, *taskC;
static struct task_struct *dl_task1, *dl_task2;

/* Forward declarations for deadlock detection threads */
static int deadlock_thread1(void *data);
static int deadlock_thread2(void *data);

static int thread_func(void *data)
{
    int prio = (int)(long)data;
    struct sched_param param = { .sched_priority = prio };
    int ret;

    // 设置实时优先级
    ret = sched_setscheduler(current, SCHED_FIFO, &param);
    if (ret)
        pr_info("Thread %s: sched_setscheduler failed: %d\n", current->comm, ret);

    pr_info("Thread %s: start, prio=%d\n", current->comm, prio);

    if (prio == 10) { // 低优先级线程C
        pr_info("Thread C: try lock\n");
        pri_mutex_lock(&test_mutex);
        pr_info("Thread C: got lock, sleep 2s\n");
        msleep(200);  /* 等待其他线程阻塞 */
        pr_info("Thread C inherited prio=%d\n", current->prio);
        msleep(1800); /* 继续持锁 */
        pr_info("Thread C: unlock\n");
        pri_mutex_unlock(&test_mutex);
    } else {
        msleep(500); // 等待C先获得锁
        pr_info("Thread %s: try lock\n", current->comm);
        pri_mutex_lock(&test_mutex);
        pr_info("Thread %s: got lock\n", current->comm);
        pri_mutex_unlock(&test_mutex);
        pr_info("Thread %s: unlock\n", current->comm);
    }
    return 0;
}

static int __init pri_mutex_test_init(void)
{
    pr_info("=== pri_mutex test module init ===\n");
    pri_mutex_init(&test_mutex);
    pri_mutex_init(&test_mutex2);

    // 创建三个线程，优先级分别为10、20、30
    taskC = kthread_run(thread_func, (void *)10, "taskC");
    taskB = kthread_run(thread_func, (void *)20, "taskB");
    taskA = kthread_run(thread_func, (void *)30, "taskA");

    /* 创建用于死锁检测的线程 */
    dl_task1 = kthread_run(deadlock_thread1, NULL, "dl1");
    dl_task2 = kthread_run(deadlock_thread2, NULL, "dl2");

    return 0;
}

static void __exit pri_mutex_test_exit(void)
{
    pr_info("=== pri_mutex test module exit ===\n");
    if (taskA)
        kthread_stop(taskA);
    if (taskB)
        kthread_stop(taskB);
    if (taskC)
        kthread_stop(taskC);
    if (dl_task1)
        kthread_stop(dl_task1);
    if (dl_task2)
        kthread_stop(dl_task2);
}

/* 死锁检测线程1：先锁test_mutex，再尝试锁test_mutex2 */
static int deadlock_thread1(void *data)
{
    int ret;
    pr_info("dl1: lock test_mutex\n");
    ret = pri_mutex_lock(&test_mutex);
    pr_info("dl1: got test_mutex, sleep\n");
    msleep(500);
    pr_info("dl1: try lock test_mutex2\n");
    ret = pri_mutex_lock(&test_mutex2);
    if (ret < 0)
        pr_info("dl1: deadlock detected err=%d\n", ret);
    return 0;
}

/* 死锁检测线程2：先锁test_mutex2，再尝试锁test_mutex */
static int deadlock_thread2(void *data)
{
    int ret;
    pr_info("dl2: lock test_mutex2\n");
    ret = pri_mutex_lock(&test_mutex2);
    pr_info("dl2: got test_mutex2, sleep\n");
    msleep(500);
    pr_info("dl2: try lock test_mutex\n");
    ret = pri_mutex_lock(&test_mutex);
    if (ret < 0)
        pr_info("dl2: deadlock detected err=%d\n", ret);
    return 0;
}

module_init(pri_mutex_test_init);
module_exit(pri_mutex_test_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("pri_mutex 优先级继承测试模块"); 