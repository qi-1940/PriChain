// rb_mutex_test.c
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include "rb_mutex.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ldfnewbie");
MODULE_DESCRIPTION("Test module for rb_mutex");

static struct task_struct *thread1;
static struct task_struct *thread2;

static struct rb_mutex test_mutex;

static int thread_fn1(void *data)
{
    pr_info("[thread1] started\n");
    rb_mutex_lock(&test_mutex);
    pr_info("[thread1] acquired lock, sleeping...\n");
    msleep(3000);  // Hold the lock for 3 seconds
    rb_mutex_unlock(&test_mutex);
    pr_info("[thread1] released lock\n");
    return 0;
}

static int thread_fn2(void *data)
{
    pr_info("[thread2] started\n");
    msleep(500);  // Wait to ensure thread1 locks first
    rb_mutex_lock(&test_mutex);
    pr_info("[thread2] acquired lock, sleeping...\n");
    msleep(1000);
    rb_mutex_unlock(&test_mutex);
    pr_info("[thread2] released lock\n");
    return 0;
}

static int __init test_init(void)
{
    pr_info("rb_mutex_test: init\n");

    rb_mutex_init(&test_mutex);

    thread1 = kthread_run(thread_fn1, NULL, "rb_mutex_t1");
    thread2 = kthread_run(thread_fn2, NULL, "rb_mutex_t2");

    return 0;
}

static void __exit test_exit(void)
{
    pr_info("rb_mutex_test: exit\n");
}

module_init(test_init);
module_exit(test_exit);
