#include <linux/pri_mutex.h>
#include <linux/slab.h>
#include <linux/sched/rt.h>
#include <linux/sched/types.h>
#include <linux/export.h>

static struct pri_mutex_waiter *top_waiter(struct pri_mutex *lock) {
    struct rb_node *node = rb_first_cached(&lock->waiters);
    if (!node)
        return NULL;
    return rb_entry(node, struct pri_mutex_waiter, node);
}

static void enqueue_waiter(struct pri_mutex *lock, struct pri_mutex_waiter *waiter) {
    struct rb_node **new = &lock->waiters.rb_root.rb_node, *parent = NULL;
    struct pri_mutex_waiter *entry;
    bool leftmost = true;
    while (*new) {
        parent = *new;
        entry = rb_entry(parent, struct pri_mutex_waiter, node);
        if (waiter->prio < entry->prio)
            new = &((*new)->rb_left);
        else {
            new = &((*new)->rb_right);
            leftmost = false;
        }
    }
    rb_link_node(&waiter->node, parent, new);
    rb_insert_color_cached(&waiter->node, &lock->waiters, leftmost);
}

static void dequeue_waiter(struct pri_mutex *lock, struct pri_mutex_waiter *waiter) {
    if (!RB_EMPTY_NODE(&waiter->node)) {
        rb_erase_cached(&waiter->node, &lock->waiters);
        RB_CLEAR_NODE(&waiter->node);
    }
}

static void boost_owner(struct pri_mutex *lock, int prio) {
    struct sched_param param;
    struct task_struct *owner = lock->owner;
    if (owner && owner->prio > prio) {
        param.sched_priority = prio;
        sched_setscheduler(owner, SCHED_FIFO, &param);
    }
}

static void restore_owner_prio(struct task_struct *owner) {
    struct sched_param param;
    param.sched_priority = owner->normal_prio;
    sched_setscheduler(owner, SCHED_FIFO, &param);
}

void pri_mutex_init(struct pri_mutex *lock) {
    lock->owner = NULL;
    lock->waiters = RB_ROOT_CACHED;
    spin_lock_init(&lock->lock);
}
EXPORT_SYMBOL(pri_mutex_init);

int pri_mutex_lock(struct pri_mutex *lock) {
    struct pri_mutex_waiter *waiter, *top;
    int ret = 0;
    waiter = kzalloc(sizeof(*waiter), GFP_KERNEL);
    if (!waiter)
        return -ENOMEM;
    waiter->task = current;
    waiter->prio = current->prio;
    waiter->lock = lock;
    RB_CLEAR_NODE(&waiter->node);
    RB_CLEAR_NODE(&waiter->pi_node);

    spin_lock(&lock->lock);
    if (lock->owner == NULL) {
        lock->owner = current;
        spin_unlock(&lock->lock);
        kfree(waiter);
        return 0;
    }
    enqueue_waiter(lock, waiter);
    boost_owner(lock, waiter->prio);
    spin_unlock(&lock->lock);

    // 阻塞等待
    for (;;) {
        set_current_state(TASK_UNINTERRUPTIBLE);
        spin_lock(&lock->lock);
        top = top_waiter(lock);
        if (lock->owner == NULL && top && top->task == current) {
            lock->owner = current;
            dequeue_waiter(lock, waiter);
            spin_unlock(&lock->lock);
            __set_current_state(TASK_RUNNING);
            break;
        }
        spin_unlock(&lock->lock);
        schedule();
    }
    kfree(waiter);
    return ret;
}
EXPORT_SYMBOL(pri_mutex_lock);

void pri_mutex_unlock(struct pri_mutex *lock) {
    struct pri_mutex_waiter *top;
    spin_lock(&lock->lock);
    restore_owner_prio(current);
    lock->owner = NULL;
    top = top_waiter(lock);
    if (top) {
        wake_up_process(top->task);
    }
    spin_unlock(&lock->lock);
}
EXPORT_SYMBOL(pri_mutex_unlock); 