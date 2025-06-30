#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/rwlock.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/completion.h>

static rwlock_t test_rwlock = __RW_LOCK_UNLOCKED(test_rwlock);
static struct task_struct *reader1, *reader2, *writer;
static struct completion r1_done, r2_done;
static u64 block_start, lock_acquire;

static void print_prio(const char *tag)
{
    pr_alert("%s: comm=%s pid=%d prio=%d normal_prio=%d\n",
             tag, current->comm, current->pid, current->prio, current->normal_prio);
}

static int reader_fn(void *data)
{
    struct timeval tv;
    int id = (int)(long)data;
    int i;
    char tag[16];

    set_user_nice(current, 10);
    snprintf(tag, sizeof(tag), "RWLOCK R%d", id);
    pr_alert("%s: START, prio=%d normal_prio=%d\n", tag, current->prio, current->normal_prio);

    do_gettimeofday(&tv);
    block_start = (u64)tv.tv_sec * 1000000000ULL + tv.tv_usec * 1000ULL;
    pr_alert("%s: Attempting read_lock at %llu ns\n", tag, block_start);

    read_lock(&test_rwlock);

    do_gettimeofday(&tv);
    lock_acquire = (u64)tv.tv_sec * 1000000000ULL + tv.tv_usec * 1000ULL;
    pr_alert("%s: GOT read_lock at %llu ns\n", tag, lock_acquire);
    pr_alert("%s: Block duration: %llu ns (%llu us)\n", tag,
             lock_acquire - block_start, (lock_acquire - block_start) / 1000ULL);

    complete(id == 1 ? &r1_done : &r2_done);

    for (i = 0; i < 10; i++) {
        udelay(1000); // busy-wait ~1ms
        print_prio(tag);
    }

    pr_alert("%s: before read_unlock, prio=%d normal_prio=%d\n", tag, current->prio, current->normal_prio);
    read_unlock(&test_rwlock);
    pr_alert("%s: RELEASE read_lock, prio=%d normal_prio=%d\n", tag, current->prio, current->normal_prio);
    return 0;
}

static int writer_fn(void *data)
{
    struct timeval tv;

    /* Ensure both readers have acquired read locks */
    wait_for_completion(&r1_done);
    wait_for_completion(&r2_done);

    set_user_nice(current, 0);
    pr_alert("RWLOCK W: START, prio=%d normal_prio=%d\n", current->prio, current->normal_prio);

    do_gettimeofday(&tv);
    block_start = (u64)tv.tv_sec * 1000000000ULL + tv.tv_usec * 1000ULL;
    pr_alert("RWLOCK W: Attempting write_lock at %llu ns\n", block_start);

    write_lock(&test_rwlock);

    do_gettimeofday(&tv);
    lock_acquire = (u64)tv.tv_sec * 1000000000ULL + tv.tv_usec * 1000ULL;
    pr_alert("RWLOCK W: GOT write_lock at %llu ns\n", lock_acquire);
    pr_alert("RWLOCK W: Block duration: %llu ns (%llu us)\n",
             lock_acquire - block_start, (lock_acquire - block_start) / 1000ULL);

    /* Hold the lock briefly */
    udelay(5000); // busy-wait ~5ms
    pr_alert("RWLOCK W: before write_unlock, prio=%d normal_prio=%d\n", current->prio, current->normal_prio);
    write_unlock(&test_rwlock);
    pr_alert("RWLOCK W: RELEASE write_lock, prio=%d normal_prio=%d\n", current->prio, current->normal_prio);
    return 0;
}

static int __init rwlock_test_init(void)
{
    pr_info("RWLOCK test loaded\n");
    init_completion(&r1_done);
    init_completion(&r2_done);

    reader1 = kthread_run(reader_fn, (void *)1L, "rwlock_r1");
    reader2 = kthread_run(reader_fn, (void *)2L, "rwlock_r2");
    writer  = kthread_run(writer_fn,  NULL,    "rwlock_w");
    return 0;
}

static void __exit rwlock_test_exit(void)
{
    pr_info("RWLOCK test unloaded\n");
}

module_init(rwlock_test_init);
module_exit(rwlock_test_exit);
MODULE_LICENSE("GPL"); 