#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/rtmutex.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/delay.h>
#include <linux/cpumask.h>
#include <linux/ktime.h>
#include <linux/completion.h>
#include <linux/rb_rwmutex.h>
#include <linux/kthread.h>

static struct rb_rwmutex test_rwlock;
static struct task_struct *task_l1, *task_l2, *task_w, *task_h;
static struct completion l1_has_lock, l2_has_lock, w_has_lock;
static u64 h_block_start_time, h_lock_acquire_time;
static u64 w_block_start_time, w_lock_acquire_time;

static void print_prio(const char *tag)
{
    pr_alert("%s: comm=%s, pid=%d, prio=%d, normal_prio=%d\n", tag, current->comm, current->pid, current->prio, current->normal_prio);
}

static void print_time(const char *tag)
{
    struct timeval tv;
    do_gettimeofday(&tv);
    pr_alert("%s: now = %llu ns\n", tag, (u64)tv.tv_sec * 1000000000ULL + (u64)tv.tv_usec * 1000ULL);
}

int low_prio_reader1(void *data)
{
    int i;
    set_user_nice(current, 10);
    pr_alert("rbrwmutex L1: START, comm=%s, pid=%d, prio=%d, normal_prio=%d\n", current->comm, current->pid, current->prio, current->normal_prio);
    print_time("rbrwmutex L1: before read lock");
    rb_rwmutex_read_lock(&test_rwlock);
    print_time("rbrwmutex L1: after read lock");
    pr_alert("rbrwmutex L1: GOT read lock, comm=%s, pid=%d, prio=%d, normal_prio=%d\n", current->comm, current->pid, current->prio, current->normal_prio);
    complete(&l1_has_lock);
    for (i = 0; i < 10; i++) {
        msleep(20);
        print_prio("rbrwmutex L1: holding read lock");
    }
    print_time("rbrwmutex L1: before read unlock");
    rb_rwmutex_read_unlock(&test_rwlock);
    print_time("rbrwmutex L1: after read unlock");
    pr_alert("rbrwmutex L1: RELEASE read lock, comm=%s, pid=%d, prio=%d, normal_prio=%d\n", current->comm, current->pid, current->prio, current->normal_prio);
    return 0;
}

int low_prio_reader2(void *data)
{
    int i;
    set_user_nice(current, 10);
    pr_alert("rbrwmutex L2: START, comm=%s, pid=%d, prio=%d, normal_prio=%d\n", current->comm, current->pid, current->prio, current->normal_prio);
    print_time("rbrwmutex L2: before read lock");
    rb_rwmutex_read_lock(&test_rwlock);
    print_time("rbrwmutex L2: after read lock");
    pr_alert("rbrwmutex L2: GOT read lock, comm=%s, pid=%d, prio=%d, normal_prio=%d\n", current->comm, current->pid, current->prio, current->normal_prio);
    complete(&l2_has_lock);
    for (i = 0; i < 15; i++) {
        msleep(20);
        print_prio("rbrwmutex L2: holding read lock");
    }
    print_time("rbrwmutex L2: before read unlock");
    rb_rwmutex_read_unlock(&test_rwlock);
    print_time("rbrwmutex L2: after read unlock");
    pr_alert("rbrwmutex L2: RELEASE read lock, comm=%s, pid=%d, prio=%d, normal_prio=%d\n", current->comm, current->pid, current->prio, current->normal_prio);
    return 0;
}

int mid_prio_writer(void *data)
{
    int i;
    set_user_nice(current, 0);
    pr_alert("rbrwmutex W: START, comm=%s, pid=%d, prio=%d, normal_prio=%d\n",
             current->comm, current->pid, current->prio, current->normal_prio);
    wait_for_completion(&l1_has_lock);
    wait_for_completion(&l2_has_lock);
    /* measure block start time */
    w_block_start_time = ktime_get_ns();
    pr_alert("rbrwmutex W: Attempting to acquire write lock at time: %llu ns\n",
             w_block_start_time);
    rb_rwmutex_write_lock(&test_rwlock);
    /* measure lock acquire time */
    w_lock_acquire_time = ktime_get_ns();
    pr_alert("rbrwmutex W: GOT write lock at time: %llu ns, prio=%d, normal_prio=%d\n",
             w_lock_acquire_time, current->prio, current->normal_prio);
    /* print block duration */
    pr_alert("rbrwmutex W: Block duration: %lld ns (%lld us)\n",
             w_lock_acquire_time - w_block_start_time,
             (w_lock_acquire_time - w_block_start_time) / 1000);
    complete(&w_has_lock);
    for (i = 0; i < 20; i++) {
        msleep(20);
        pr_alert("rbrwmutex W: holding write lock, prio=%d, normal_prio=%d\n",
                 current->prio, current->normal_prio);
    }
    /* release write lock */
    rb_rwmutex_write_unlock(&test_rwlock);
    pr_alert("rbrwmutex W: RELEASE write lock, prio=%d, normal_prio=%d\n",
             current->prio, current->normal_prio);
    return 0;
}

int high_prio_reader(void *data)
{
    int i;
    s64 block_duration;
    struct timeval tv;
    u64 start_ns, end_ns;
    set_user_nice(current, -20);
    pr_alert("rbrwmutex H: START, comm=%s, pid=%d, prio=%d, normal_prio=%d\n", current->comm, current->pid, current->prio, current->normal_prio);
    wait_for_completion(&w_has_lock);
    do_gettimeofday(&tv);
    start_ns = (u64)tv.tv_sec * 1000000000ULL + (u64)tv.tv_usec * 1000ULL;
    pr_alert("rbrwmutex H: Attempting to acquire read lock at time: %llu ns, comm=%s, pid=%d, prio=%d, normal_prio=%d\n",
             start_ns, current->comm, current->pid, current->prio, current->normal_prio);
    rb_rwmutex_read_lock(&test_rwlock);
    do_gettimeofday(&tv);
    end_ns = (u64)tv.tv_sec * 1000000000ULL + (u64)tv.tv_usec * 1000ULL;
    pr_alert("###\n");
    pr_alert("rbrwmutex H: GOT read lock, comm=%s, pid=%d, prio=%d, normal_prio=%d\n", current->comm, current->pid, current->prio, current->normal_prio);
    pr_alert("rbrwmutex H: Acquired read lock at time: %llu ns\n", end_ns);
    pr_alert("###\n");
    block_duration = end_ns - start_ns;
    pr_alert("rbrwmutex H: Block duration: %lld ns (%lld us)\n", block_duration, block_duration / 1000);
    for (i = 0; i < 5; i++) {
        msleep(20);
        print_prio("rbrwmutex H: holding read lock");
    }
    print_time("rbrwmutex H: before read unlock");
    rb_rwmutex_read_unlock(&test_rwlock);
    print_time("rbrwmutex H: after read unlock");
    pr_alert("rbrwmutex H: RELEASE read lock, comm=%s, pid=%d, prio=%d, normal_prio=%d\n", current->comm, current->pid, current->prio, current->normal_prio);
    return 0;
}

static int __init rbrwmutex_test_init(void)
{
    pr_info("rbrwmutex detailed log test module loaded\n");
    rb_rwmutex_init(&test_rwlock);
    init_completion(&l1_has_lock);
    init_completion(&l2_has_lock);
    init_completion(&w_has_lock);
    task_l1 = kthread_run(low_prio_reader1, NULL, "rw_reader1");
    if (IS_ERR(task_l1)) return PTR_ERR(task_l1);
    task_l2 = kthread_run(low_prio_reader2, NULL, "rw_reader2");
    if (IS_ERR(task_l2)) return PTR_ERR(task_l2);
    task_w = kthread_run(mid_prio_writer, NULL, "rw_writer");
    if (IS_ERR(task_w)) return PTR_ERR(task_w);
    task_h = kthread_run(high_prio_reader, NULL, "rw_readerH");
    if (IS_ERR(task_h)) return PTR_ERR(task_h);
    return 0;
}

static void __exit rbrwmutex_test_exit(void)
{
    pr_info("rbrwmutex test module unloaded\n");
}

module_init(rbrwmutex_test_init);
module_exit(rbrwmutex_test_exit);
MODULE_LICENSE("GPL");