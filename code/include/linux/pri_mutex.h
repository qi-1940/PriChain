#ifndef _LINUX_PRI_MUTEX_H
#define _LINUX_PRI_MUTEX_H

#include <linux/rbtree.h>
#include <linux/spinlock.h>
#include <linux/sched.h>

struct pri_mutex;

/**
 * struct pri_mutex_waiter - Waiter structure for priority inheritance mutex
 * @task:       The task waiting for the mutex
 * @node:       RB-tree node for the lock's waiters tree
 * @prio:       Priority of the waiting task
 * @deadline:   Deadline of the waiting task
 * @lock:       The mutex being waited on
 * @pi_node:    RB-tree node for the owner's pi_waiters tree
 */
struct pri_mutex_waiter {
    struct task_struct *task;           // 等待该锁的任务
    struct rb_node node;                // 用于锁的waiters队列（红黑树节点）
    int prio;                           // 等待者优先级
    u64 deadline;                       // 等待者deadline，用于EDF支持
    struct pri_mutex *lock;             // 正在等待的锁
    struct rb_node pi_node;             // 用于任务的pi_waiters树（红黑树节点）
};

/**
 * struct pri_mutex - Priority inheritance mutex structure
 * @owner:      The current owner of the mutex
 * @waiters:    RB-tree of waiting tasks, sorted by priority
 * @lock:       Spinlock to protect the mutex structure
 */
struct pri_mutex {
    struct task_struct *owner;          // 当前持有锁的任务
    struct rb_root_cached waiters;      // 等待队列，按优先级排序（红黑树）
    spinlock_t lock;                    // 自旋锁保护
};

#define PRI_MUTEX_HAS_WAITERS 1UL

enum pri_mutex_chainwalk {
    PRI_MUTEX_MIN_CHAINWALK,
    PRI_MUTEX_FULL_CHAINWALK,
};

void pri_mutex_init(struct pri_mutex *lock);
int pri_mutex_lock(struct pri_mutex *lock);
void pri_mutex_unlock(struct pri_mutex *lock);

#endif 