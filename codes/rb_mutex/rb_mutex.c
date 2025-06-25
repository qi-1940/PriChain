#include <linux/sched.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/wait.h>
#include <linux/hashtable.h>
#include <linux/jiffies.h>
#include <linux/errno.h>
#include <linux/sched/signal.h>
#include <linux/sched/types.h>
#include <linux/sched/rt.h>
#include <linux/sched/deadline.h>
#include <linux/sched/task.h>
#include <linux/smp.h>
#include "rb_mutex.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ldfnewbie");
MODULE_DESCRIPTION("RB-Tree Mutex System with Dynamic Instances, Priority Inheritance, and Exported Interface");

#define RB_TASK_TABLE_BITS 6
static DEFINE_HASHTABLE(rb_task_table, RB_TASK_TABLE_BITS);
static DEFINE_SPINLOCK(rb_task_table_lock);

static struct rb_task_info *get_task_info(struct task_struct *task)
{
    struct rb_task_info *info;
    hash_for_each_possible(rb_task_table, info, hnode, (unsigned long)task) {
        if (info->task == task)
            return info;
    }
    return NULL;
}

static struct rb_task_info *ensure_task_info(struct task_struct *task)
{
    struct rb_task_info *info;
    unsigned long flags;

    spin_lock_irqsave(&rb_task_table_lock, flags);
    info = get_task_info(task);
    if (!info) {
        info = kmalloc(sizeof(*info), GFP_ATOMIC);
        if (info) {
            info->task = task;
            info->waiting_on = NULL;
            info->original_prio = -1;
            hash_add(rb_task_table, &info->hnode, (unsigned long)task);
        }
    }
    spin_unlock_irqrestore(&rb_task_table_lock, flags);
    return info;
}

static void rb_mutex_enqueue(struct rb_mutex *mutex, struct rb_mutex_waiter *waiter)
{
    struct rb_node **new = &mutex->waiters.rb_node, *parent = NULL;
    struct rb_mutex_waiter *entry;

    while (*new) {
        parent = *new;
        entry = rb_entry(parent, struct rb_mutex_waiter, node);
        if (waiter->prio > entry->prio)
            new = &(*new)->rb_left;
        else
            new = &(*new)->rb_right;
    }

    rb_link_node(&waiter->node, parent, new);
    rb_insert_color(&waiter->node, &mutex->waiters);
}

static struct rb_mutex_waiter *rb_mutex_top_waiter(struct rb_mutex *mutex)
{
    struct rb_node *node = mutex->waiters.rb_node;
    if (!node)
        return NULL;
    while (node->rb_left)
        node = node->rb_left;
    return rb_entry(node, struct rb_mutex_waiter, node);
}

static void rb_mutex_dequeue(struct rb_mutex *mutex, struct rb_mutex_waiter *waiter)
{
    rb_erase(&waiter->node, &mutex->waiters);
    waiter->node.rb_left = waiter->node.rb_right = NULL;
}

static void propagate_priority(struct task_struct *owner, int prio)
{
    struct rb_task_info *info = ensure_task_info(owner);
    if (!info) return;

    if (owner->prio > prio) {
        if (info->original_prio < 0)
            info->original_prio = owner->prio;

        owner->prio = prio;
        pr_info("[rb_mutex] %s inherits prio %d\n", owner->comm, prio);

        // ✅ 启发调度器重新考虑调度
        set_tsk_need_resched(owner);   // 标记需要调度
        wake_up_process(owner);        // 如果在休眠，则唤醒立即重新调度

        if (info->waiting_on && info->waiting_on->owner)
            propagate_priority(info->waiting_on->owner, prio);
    }
}
static void restore_priority(struct task_struct *task)
{
    struct rb_task_info *info = get_task_info(task);
    if (!info) return;

    if (info->original_prio >= 0 && task->prio != info->original_prio) {
        pr_info("[rb_mutex] restore prio for %s to %d\n", task->comm, info->original_prio);
        task->prio = info->original_prio;
        info->original_prio = -1;

        // 同样提示调度器
        set_tsk_need_resched(task);
        wake_up_process(task);
    }
}
void rb_mutex_init(struct rb_mutex *mutex)
{
    mutex->owner = NULL;
    mutex->waiters = RB_ROOT;
    spin_lock_init(&mutex->lock);
    init_waitqueue_head(&mutex->wait_queue);
}

void rb_mutex_lock(struct rb_mutex *mutex)
{
    DEFINE_WAIT(wait);
    struct rb_mutex_waiter self;
    struct rb_task_info *info = ensure_task_info(current);

    self.task = current;
    self.prio = current->prio;
    INIT_LIST_HEAD(&self.list);

    spin_lock(&mutex->lock);
    if (!mutex->owner) {
        mutex->owner = current;
        if (info) info->waiting_on = NULL;
        spin_unlock(&mutex->lock);
        return;
    }

    rb_mutex_enqueue(mutex, &self);
    if (info) info->waiting_on = mutex;
    propagate_priority(mutex->owner, current->prio);
    spin_unlock(&mutex->lock);

    for (;;) {
        prepare_to_wait(&mutex->wait_queue, &wait, TASK_UNINTERRUPTIBLE);
        spin_lock(&mutex->lock);
        if (mutex->owner == NULL && rb_mutex_top_waiter(mutex) == &self) {
            rb_mutex_dequeue(mutex, &self);
            mutex->owner = current;
            if (info) info->waiting_on = NULL;
            spin_unlock(&mutex->lock);
            break;
        }
        spin_unlock(&mutex->lock);
        schedule();
    }
    finish_wait(&mutex->wait_queue, &wait);
}

void rb_mutex_unlock(struct rb_mutex *mutex)
{
    spin_lock(&mutex->lock);
    if (mutex->owner != current) {
        spin_unlock(&mutex->lock);
        return;
    }
    mutex->owner = NULL;
    restore_priority(current);

    if (!RB_EMPTY_ROOT(&mutex->waiters)) {
        struct rb_mutex_waiter *next = rb_mutex_top_waiter(mutex);
        wake_up_process(next->task);
    }
    spin_unlock(&mutex->lock);
}

int rb_mutex_trylock(struct rb_mutex *mutex)
{
    int success = 0;
    spin_lock(&mutex->lock);
    if (!mutex->owner) {
        mutex->owner = current;
        struct rb_task_info *info = ensure_task_info(current);
        if (info) info->waiting_on = NULL;
        success = 1;
    }
    spin_unlock(&mutex->lock);
    return success;
}

int rb_mutex_lock_timeout(struct rb_mutex *mutex, unsigned long timeout_ms)
{
    DEFINE_WAIT(wait);
    struct rb_mutex_waiter self;
    struct rb_task_info *info = ensure_task_info(current);
    unsigned long deadline = jiffies + msecs_to_jiffies(timeout_ms);

    if (mutex->owner == current) {
        pr_warn("[rb_mutex] Deadlock detected: %s tried to re-lock\n", current->comm);
        return -EDEADLK;
    }

    self.task = current;
    self.prio = current->prio;
    INIT_LIST_HEAD(&self.list);

    spin_lock(&mutex->lock);
    if (!mutex->owner) {
        mutex->owner = current;
        if (info) info->waiting_on = NULL;
        spin_unlock(&mutex->lock);
        return 0;
    }

    rb_mutex_enqueue(mutex, &self);
    if (info) info->waiting_on = mutex;
    propagate_priority(mutex->owner, current->prio);
    spin_unlock(&mutex->lock);

    for (;;) {
        prepare_to_wait(&mutex->wait_queue, &wait, TASK_UNINTERRUPTIBLE);

        spin_lock(&mutex->lock);
        if (mutex->owner == NULL && rb_mutex_top_waiter(mutex) == &self) {
            rb_mutex_dequeue(mutex, &self);
            mutex->owner = current;
            if (info) info->waiting_on = NULL;
            spin_unlock(&mutex->lock);
            break;
        }
        spin_unlock(&mutex->lock);

        if (time_after(jiffies, deadline)) {
            rb_mutex_dequeue(mutex, &self);
            if (info) info->waiting_on = NULL;
            finish_wait(&mutex->wait_queue, &wait);
            return -ETIMEDOUT;
        }

        schedule_timeout(msecs_to_jiffies(10));
    }

    finish_wait(&mutex->wait_queue, &wait);
    return 0;
}

static int __init rb_mutex_module_init(void)
{
    pr_info("rb_mutex kernel module loaded\n");
    return 0;
}

static void __exit rb_mutex_module_exit(void)
{
    pr_info("rb_mutex kernel module unloaded\n");
}

module_init(rb_mutex_module_init);
module_exit(rb_mutex_module_exit);

EXPORT_SYMBOL(rb_mutex_init);
EXPORT_SYMBOL(rb_mutex_lock);
EXPORT_SYMBOL(rb_mutex_unlock);
EXPORT_SYMBOL(rb_mutex_trylock);
EXPORT_SYMBOL(rb_mutex_lock_timeout);
