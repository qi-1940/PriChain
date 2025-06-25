#ifndef _RB_MUTEX_H
#define _RB_MUTEX_H

#include <linux/rbtree.h>

struct rb_mutex; // 前向声明是为了交叉引用，但不是完整定义

struct task_node {
    struct rb_node node;
    char name[16];
    int base_prio;
    int curr_prio;
    struct rb_mutex *waiting_on;  // 引用等待的互斥锁
};

struct rb_mutex {
    struct task_node *owner;
    struct rb_root waiters;
};

// 函数声明
struct task_node *alloc_task(const char *name, int prio);
void rb_mutex_lock(struct rb_mutex *mutex, struct task_node *task);
void rb_mutex_unlock(struct rb_mutex *mutex);
void reset_priority(struct task_node *task);

#endif
