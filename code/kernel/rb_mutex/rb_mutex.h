// rb_mutex.h
#ifndef _RB_MUTEX_H
#define _RB_MUTEX_H

#include <linux/rbtree.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/list.h>
#include <linux/hashtable.h>

struct rb_mutex {
    struct task_struct *owner;
    struct rb_root waiters;
    spinlock_t lock;
    wait_queue_head_t wait_queue;
};

struct rb_mutex_waiter {
    struct task_struct *task;
    int prio;
    struct list_head list;
    struct rb_node node;
};

struct rb_task_info {
    struct task_struct *task;
    int original_prio;
    struct rb_mutex *waiting_on;
    struct hlist_node hnode;
};

void rb_mutex_init(struct rb_mutex *mutex);
void rb_mutex_lock(struct rb_mutex *mutex);
void rb_mutex_unlock(struct rb_mutex *mutex);
int rb_mutex_trylock(struct rb_mutex *mutex);
int rb_mutex_lock_timeout(struct rb_mutex *mutex, unsigned long timeout_ms);
#endif /* _RB_MUTEX_H */
