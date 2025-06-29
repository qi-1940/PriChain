#include <linux/rb_rwsem.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/sched.h>

static struct rb_rwsem test_rwsem;
static struct task_struct *reader, *writer;

static int reader_fn(void *data)
{
    set_user_nice(current, 10); // 低优先级
    pr_alert("[RWSEM-PI] Reader: try write lock, prio=%d\n", current->prio);
    rb_rwsem_down_write(&test_rwsem);
    pr_alert("[RWSEM-PI] Reader: got write lock, prio=%d, will sleep 1s\n", current->prio);
    msleep(1000);
    pr_alert("[RWSEM-PI] Reader: releasing write lock, prio=%d\n", current->prio);
    rb_rwsem_up_write(&test_rwsem);
    pr_alert("[RWSEM-PI] Reader: released write lock, prio=%d\n", current->prio);
    return 0;
}

static int writer_fn(void *data)
{
    msleep(200); // 保证Reader先获得锁
    set_user_nice(current, -20); // 高优先级
    pr_alert("[RWSEM-PI] Writer: try write lock, prio=%d (should block)\n", current->prio);
    rb_rwsem_down_write(&test_rwsem);
    pr_alert("[RWSEM-PI] Writer: got write lock, prio=%d\n", current->prio);
    rb_rwsem_up_write(&test_rwsem);
    pr_alert("[RWSEM-PI] Writer: released write lock, prio=%d\n", current->prio);
    return 0;
}

static int __init rwsem_test_init(void)
{
    pr_info("[RWSEM-PI] rb_rwsem priority inheritance test loaded\n");
    rb_rwsem_init(&test_rwsem);
    reader = kthread_run(reader_fn, NULL, "rwsem_reader");
    writer = kthread_run(writer_fn, NULL, "rwsem_writer");
    return 0;
}

static void __exit rwsem_test_exit(void)
{
    pr_info("[RWSEM-PI] rb_rwsem priority inheritance test unloaded\n");
}

module_init(rwsem_test_init);
module_exit(rwsem_test_exit);
MODULE_LICENSE("GPL"); 