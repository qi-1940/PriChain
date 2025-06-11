#ifndef _LINUX_PRI_MUTEX_H
#define _LINUX_PRI_MUTEX_H

#include <linux/rbtree.h>
#include <linux/spinlock.h>
#include <linux/sched.h>

struct pri_mutex;

struct pri_mutex_waiter {
    struct task_struct *task;
    struct rb_node node;
    int prio;
    struct pri_mutex *lock;
    struct rb_node pi_node;
};

struct pri_mutex {
    struct task_struct *owner;
    struct rb_root_cached waiters;
    spinlock_t lock;
};

void pri_mutex_init(struct pri_mutex *lock);
int pri_mutex_lock(struct pri_mutex *lock);
void pri_mutex_unlock(struct pri_mutex *lock);

#endif 