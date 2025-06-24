#ifndef _RB_MUTEX_H
#define _RB_MUTEX_H

#include <linux/rbtree.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/wait.h>

// 等待者结构：记录任务和优先级，用于红黑树调度
struct rb_mutex_waiter {
    struct rb_node node;
    struct task_struct *task;
    int prio;
    struct list_head list;
};

// 互斥锁结构：包括所有必要状态
struct rb_mutex {
    wait_queue_head_t wait_queue;
    struct task_struct *owner;
    struct rb_root waiters;
    spinlock_t lock;
};

// 接口函数声明
void rb_mutex_init(struct rb_mutex *mutex);
void rb_mutex_lock(struct rb_mutex *mutex);
void rb_mutex_unlock(struct rb_mutex *mutex);
int  rb_mutex_trylock(struct rb_mutex *mutex);

#endif // _RB_MUTEX_H
