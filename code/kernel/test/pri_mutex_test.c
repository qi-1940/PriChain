#include <linux/module.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/sched/types.h>
#include <linux/delay.h>
#include <linux/pri_mutex.h> // 你的pri_mutex头文件

static struct pri_mutex test_mutex;
static struct task_struct *taskA, *taskB, *taskC;

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
        msleep(2000); // 持锁2秒
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

    // 创建三个线程，优先级分别为10、20、30
    taskC = kthread_run(thread_func, (void *)10, "taskC");
    taskB = kthread_run(thread_func, (void *)20, "taskB");
    taskA = kthread_run(thread_func, (void *)30, "taskA");

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
}

module_init(pri_mutex_test_init);
module_exit(pri_mutex_test_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("AI助手");
MODULE_DESCRIPTION("pri_mutex 优先级继承测试模块"); 