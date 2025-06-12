// SPDX-License-Identifier: GPL-2.0
#include <linux/pri_mutex.h>
#include <linux/slab.h>
#include <linux/sched/rt.h>
#include <linux/sched/types.h>
#include <linux/export.h>
#include <linux/sched.h>
#define PRI_MUTEX_MAX_LOCK_DEPTH 1024
/* Forward declarations for helper functions */
static struct pri_mutex_waiter *pri_mutex_top_waiter(struct pri_mutex *lock);
static void enqueue_waiter(struct pri_mutex *lock, struct pri_mutex_waiter *waiter);
static void dequeue_waiter(struct pri_mutex *lock, struct pri_mutex_waiter *waiter);
static void enqueue_pi_waiter(struct task_struct *owner, struct pri_mutex_waiter *waiter);
static void dequeue_pi_waiter(struct task_struct *owner, struct pri_mutex_waiter *waiter);
static void propagate_prio(struct task_struct *owner);
static bool detect_deadlock(struct task_struct *task, struct pri_mutex *lock);

/*
 * Priority Inheritance Mutex implementation
 * This lock allows multiple threads with different priorities to wait for the same lock.
 * The owner of the lock will inherit the highest priority among the waiters.
 */

/* Initialize the priority inheritance mutex */
void pri_mutex_init(struct pri_mutex *lock) {
    lock->owner = NULL;
    lock->waiters = RB_ROOT_CACHED;
    spin_lock_init(&lock->lock);
}
EXPORT_SYMBOL(pri_mutex_init);

/* Add fast/slow path implementations */
static inline bool try_to_take_pri_mutex(struct pri_mutex *lock,
                                         struct task_struct *task,
                                         struct pri_mutex_waiter *waiter)
{
    bool ret = false;
    unsigned long flags;

    spin_lock_irqsave(&lock->lock, flags);
    if (lock->owner == NULL) {
        lock->owner = task;
        ret = true;
    } else if (waiter && pri_mutex_top_waiter(lock) == waiter) {
        dequeue_waiter(lock, waiter);
        dequeue_pi_waiter(task, waiter);
        task->pri_blocked_on = NULL;
        lock->owner = task;
        ret = true;
    }
    spin_unlock_irqrestore(&lock->lock, flags);
    return ret;
}

static int task_blocks_on_pri_mutex(struct pri_mutex *lock,
                                    struct pri_mutex_waiter *waiter,
                                    struct task_struct *task,
                                    enum pri_mutex_chainwalk chain)
{
    unsigned long flags;
    struct task_struct *owner;

    spin_lock_irqsave(&lock->lock, flags);
    owner = lock->owner;
    if (owner == task || detect_deadlock(task, lock)) {
        spin_unlock_irqrestore(&lock->lock, flags);
        return -EDEADLK;
    }

    enqueue_waiter(lock, waiter);
    task->pri_blocked_on = waiter;
    enqueue_pi_waiter(owner, waiter);
    propagate_prio(owner);
    spin_unlock_irqrestore(&lock->lock, flags);

    return 0;
}

static int __pri_mutex_slowlock(struct pri_mutex *lock)
{
    struct pri_mutex_waiter waiter;
    int ret;

    waiter.task = current;
    waiter.prio = current->prio;
    waiter.deadline = 0;
    waiter.lock = lock;
    RB_CLEAR_NODE(&waiter.node);
    RB_CLEAR_NODE(&waiter.pi_node);

    if (try_to_take_pri_mutex(lock, current, NULL))
        return 0;

    ret = task_blocks_on_pri_mutex(lock, &waiter, current, PRI_MUTEX_FULL_CHAINWALK);
    if (ret)
        return ret;

    for (;;) {
        set_current_state(TASK_UNINTERRUPTIBLE);
        if (try_to_take_pri_mutex(lock, current, &waiter))
            break;
        schedule();
    }
    __set_current_state(TASK_RUNNING);
    return 0;
}

int pri_mutex_lock(struct pri_mutex *lock)
{
    if (try_to_take_pri_mutex(lock, current, NULL))
        return 0;
    return __pri_mutex_slowlock(lock);
}
EXPORT_SYMBOL(pri_mutex_lock);

void pri_mutex_unlock(struct pri_mutex *lock) {
    struct sched_param param;
    struct pri_mutex_waiter *top;
    unsigned long flags;

    spin_lock_irqsave(&lock->lock, flags);
    param.sched_priority = current->normal_prio;
    sched_setscheduler(current, SCHED_FIFO, &param);
    current->pri_blocked_on = NULL;
    lock->owner = NULL;
    top = pri_mutex_top_waiter(lock);
    if (top) {
        dequeue_waiter(lock, top);
        dequeue_pi_waiter(current, top);
        wake_up_process(top->task);
    }
    spin_unlock_irqrestore(&lock->lock, flags);
}
EXPORT_SYMBOL(pri_mutex_unlock);

// Get the top waiter (highest priority) from lock's waiters
static struct pri_mutex_waiter *pri_mutex_top_waiter(struct pri_mutex *lock)
{
    struct rb_node *node = rb_first_cached(&lock->waiters);
    return node ? rb_entry(node, struct pri_mutex_waiter, node) : NULL;
}

// Enqueue a waiter into lock's waiters RB-tree
static void enqueue_waiter(struct pri_mutex *lock, struct pri_mutex_waiter *waiter)
{
    struct rb_node **link = &lock->waiters.rb_root.rb_node, *parent = NULL;
    struct pri_mutex_waiter *entry;
    bool leftmost = true;

    while (*link) {
        parent = *link;
        entry = rb_entry(parent, struct pri_mutex_waiter, node);
        if (waiter->prio < entry->prio ||
           (waiter->prio == entry->prio && waiter->deadline < entry->deadline))
            link = &parent->rb_left;
        else {
            link = &parent->rb_right;
            leftmost = false;
        }
    }

    rb_link_node(&waiter->node, parent, link);
    rb_insert_color_cached(&waiter->node, &lock->waiters, leftmost);
}

// Dequeue a waiter from lock's waiters RB-tree
static void dequeue_waiter(struct pri_mutex *lock, struct pri_mutex_waiter *waiter)
{
    if (!RB_EMPTY_NODE(&waiter->node)) {
        rb_erase_cached(&waiter->node, &lock->waiters);
        RB_CLEAR_NODE(&waiter->node);
    }
}

// Enqueue a waiter into owner's pi_waiters RB-tree
static void enqueue_pi_waiter(struct task_struct *owner, struct pri_mutex_waiter *waiter)
{
    struct rb_node **link = &owner->pi_waiters.rb_root.rb_node, *parent = NULL;
    struct pri_mutex_waiter *entry;
    bool leftmost = true;

    while (*link) {
        parent = *link;
        entry = rb_entry(parent, struct pri_mutex_waiter, pi_node);
        if (waiter->prio < entry->prio ||
           (waiter->prio == entry->prio && waiter->deadline < entry->deadline))
            link = &parent->rb_left;
        else {
            link = &parent->rb_right;
            leftmost = false;
        }
    }

    rb_link_node(&waiter->pi_node, parent, link);
    rb_insert_color_cached(&waiter->pi_node, &owner->pi_waiters, leftmost);
}

// Dequeue a waiter from owner's pi_waiters RB-tree
static void dequeue_pi_waiter(struct task_struct *owner, struct pri_mutex_waiter *waiter)
{
    if (!RB_EMPTY_NODE(&waiter->pi_node)) {
        rb_erase_cached(&waiter->pi_node, &owner->pi_waiters);
        RB_CLEAR_NODE(&waiter->pi_node);
    }
}

// Recursively propagate priority up the owner chain
static void propagate_prio(struct task_struct *owner)
{
    int max_prio = owner->prio;
    struct rb_node *node = rb_first_cached(&owner->pi_waiters);

    while (node) {
        struct pri_mutex_waiter *w = rb_entry(node, struct pri_mutex_waiter, pi_node);
        if (w->prio < max_prio)
            max_prio = w->prio;
        node = rb_next(node);
    }

    if (max_prio < owner->prio) {
        struct sched_param param = { .sched_priority = max_prio };
        sched_setscheduler(owner, SCHED_FIFO, &param);
        if (owner->pri_blocked_on && owner->pri_blocked_on->lock->owner)
            propagate_prio(owner->pri_blocked_on->lock->owner);
    }
}

// Deadlock detection: walk the owner chain to detect cycles
static bool detect_deadlock(struct task_struct *task, struct pri_mutex *lock)
{
    struct task_struct *owner = lock->owner;
    int depth = 0;

    while (owner) {
        if (owner == task)
            return true;
        if (++depth > PRI_MUTEX_MAX_LOCK_DEPTH)
            return true;
        if (!owner->pri_blocked_on)
            break;
        owner = owner->pri_blocked_on->lock->owner;
    }

    return false;
} 
