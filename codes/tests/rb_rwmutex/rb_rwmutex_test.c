#include <linux/rb_rwmutex.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/sched.h>

static struct rb_rwmutex test_rwlock;
static struct task_struct *reader, *writer;

static int reader_fn(void *data)
{
    set_user_nice(current, 10); // 设置低优先级
    pr_alert("[RWTEST-PI] Reader: try write lock, prio=%d\n", current->prio);
    rb_rwmutex_write_lock(&test_rwlock);
    pr_alert("[RWTEST-PI] Reader: got write lock, prio=%d, will sleep 1s\n", current->prio);
    msleep(1000); // Writer会被阻塞，期间观察优先级提升
    pr_alert("[RWTEST-PI] Reader: releasing write lock, prio=%d\n", current->prio);
    rb_rwmutex_write_unlock(&test_rwlock);
    pr_alert("[RWTEST-PI] Reader: released write lock, prio=%d\n", current->prio);
    return 0;
}

static int writer_fn(void *data)
{
    msleep(200); // 保证Reader先获得锁
    set_user_nice(current, -20); // 设置高优先级
    pr_alert("[RWTEST-PI] Writer: try write lock, prio=%d (should block)\n", current->prio);
    rb_rwmutex_write_lock(&test_rwlock);
    pr_alert("[RWTEST-PI] Writer: got write lock, prio=%d\n", current->prio);
    rb_rwmutex_write_unlock(&test_rwlock);
    pr_alert("[RWTEST-PI] Writer: released write lock, prio=%d\n", current->prio);
    return 0;
}

static int __init rwmutex_test_init(void)
{
    pr_info("[RWTEST-PI] rb_rwmutex priority inheritance test loaded\n");
    rb_rwmutex_init(&test_rwlock);
    reader = kthread_run(reader_fn, NULL, "rw_reader");
    writer = kthread_run(writer_fn, NULL, "rw_writer");
    return 0;
}

static void __exit rwmutex_test_exit(void)
{
    pr_info("[RWTEST-PI] rb_rwmutex priority inheritance test unloaded\n");
}

module_init(rwmutex_test_init);
module_exit(rwmutex_test_exit);
MODULE_LICENSE("GPL"); 