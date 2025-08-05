#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/time.h>
#include <linux/completion.h>
#include <linux/rwsem.h>

static struct rw_semaphore test_rwsem;
static struct task_struct *reader1, *reader2, *writer;
static struct completion r1_done, r2_done;
static u64 block_start, lock_acquire;

static void print_prio(const char *tag)
{
    pr_alert("%s: comm=%s pid=%d prio=%d normal_prio=%d\n",
             tag, current->comm, current->pid, current->prio, current->normal_prio);
}

static int reader1_fn(void *data)
{
    struct timeval tv;
    int i;
    set_user_nice(current, 10);
    pr_alert("RWSEM R1: START, prio=%d normal_prio=%d\n", current->prio, current->normal_prio);
    do_gettimeofday(&tv);
    block_start = (u64)tv.tv_sec * 1000000000ULL + (u64)tv.tv_usec * 1000ULL;
    pr_alert("RWSEM R1: Attempting down_read at %llu ns\n", block_start);
    down_read(&test_rwsem);
    do_gettimeofday(&tv);
    lock_acquire = (u64)tv.tv_sec * 1000000000ULL + (u64)tv.tv_usec * 1000ULL;
    pr_alert("RWSEM R1: GOT read lock at %llu ns\n", lock_acquire);
    pr_alert("RWSEM R1: Post-lock prio=%d normal_prio=%d\n", current->prio, current->normal_prio);
    pr_alert("RWSEM R1: Block duration: %llu ns (%llu us)\n",
             lock_acquire - block_start, (lock_acquire - block_start) / 1000);
    complete(&r1_done);
    for (i = 0; i < 10; i++) {
        msleep(100);
        pr_alert("RWSEM R1: holding read lock, prio=%d normal_prio=%d\n", current->prio, current->normal_prio);
    }
    pr_alert("RWSEM R1: before up_read, prio=%d normal_prio=%d\n", current->prio, current->normal_prio);
    up_read(&test_rwsem);
    pr_alert("RWSEM R1: RELEASE read lock, prio=%d normal_prio=%d\n", current->prio, current->normal_prio);
    return 0;
}

static int reader2_fn(void *data)
{
    struct timeval tv;
    int i;
    set_user_nice(current, 10);
    pr_alert("RWSEM R2: START, prio=%d normal_prio=%d\n", current->prio, current->normal_prio);
    do_gettimeofday(&tv);
    block_start = (u64)tv.tv_sec * 1000000000ULL + (u64)tv.tv_usec * 1000ULL;
    pr_alert("RWSEM R2: Attempting down_read at %llu ns\n", block_start);
    down_read(&test_rwsem);
    do_gettimeofday(&tv);
    lock_acquire = (u64)tv.tv_sec * 1000000000ULL + (u64)tv.tv_usec * 1000ULL;
    pr_alert("RWSEM R2: GOT read lock at %llu ns\n", lock_acquire);
    pr_alert("RWSEM R2: Post-lock prio=%d normal_prio=%d\n", current->prio, current->normal_prio);
    pr_alert("RWSEM R2: Block duration: %llu ns (%llu us)\n",
             lock_acquire - block_start, (lock_acquire - block_start) / 1000);
    complete(&r2_done);
    for (i = 0; i < 10; i++) {
        msleep(100);
        pr_alert("RWSEM R2: holding read lock, prio=%d normal_prio=%d\n", current->prio, current->normal_prio);
    }
    pr_alert("RWSEM R2: before up_read, prio=%d normal_prio=%d\n", current->prio, current->normal_prio);
    up_read(&test_rwsem);
    pr_alert("RWSEM R2: RELEASE read lock, prio=%d normal_prio=%d\n", current->prio, current->normal_prio);
    return 0;
}

static int writer_fn(void *data)
{
    struct timeval tv;
    pr_alert("RWSEM W: START, prio=%d normal_prio=%d\n", current->prio, current->normal_prio);
    do_gettimeofday(&tv);
    block_start = (u64)tv.tv_sec * 1000000000ULL + (u64)tv.tv_usec * 1000ULL;
    pr_alert("RWSEM W: Attempting down_write at %llu ns\n", block_start);
    down_write(&test_rwsem);
    do_gettimeofday(&tv);
    lock_acquire = (u64)tv.tv_sec * 1000000000ULL + (u64)tv.tv_usec * 1000ULL;
    pr_alert("RWSEM W: GOT write lock at %llu ns\n", lock_acquire);
    pr_alert("RWSEM W: Post-lock prio=%d normal_prio=%d\n", current->prio, current->normal_prio);
    pr_alert("RWSEM W: Block duration: %llu ns (%llu us)\n",
             lock_acquire - block_start, (lock_acquire - block_start) / 1000);
    /* hold write lock briefly */
    msleep(500);
    pr_alert("RWSEM W: releasing write lock, prio=%d normal_prio=%d\n", current->prio, current->normal_prio);
    up_write(&test_rwsem);
    pr_alert("RWSEM W: RELEASE write lock, prio=%d normal_prio=%d\n", current->prio, current->normal_prio);
    return 0;
}

static int __init rwsem_test_init(void)
{
    pr_info("RWSEM test loaded\n");
    init_rwsem(&test_rwsem);
    init_completion(&r1_done);
    init_completion(&r2_done);
    reader1 = kthread_run(reader1_fn, NULL, "rwsem_r1");
    if (IS_ERR(reader1)) return PTR_ERR(reader1);
    reader2 = kthread_run(reader2_fn, NULL, "rwsem_r2");
    if (IS_ERR(reader2)) return PTR_ERR(reader2);
    writer = kthread_run(writer_fn, NULL, "rwsem_w");
    if (IS_ERR(writer)) return PTR_ERR(writer);
    return 0;
}

static void __exit rwsem_test_exit(void)
{
    pr_info("RWSEM test unloaded\n");
}

module_init(rwsem_test_init);
module_exit(rwsem_test_exit);
MODULE_LICENSE("GPL"); 