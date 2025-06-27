#ifndef RB_MUTEX_H
#define RB_MUTEX_H

#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/rbtree.h>
#include <linux/sched.h>
#include <linux/hashtable.h>

// 互斥锁等待者结构
struct rb_mutex_waiter {
    struct task_struct *task;
    int prio;
    struct rb_node node;
    struct list_head list;
};

// 互斥锁结构
struct rb_mutex {
    struct task_struct *owner;
    struct rb_root waiters;
    spinlock_t lock;
    wait_queue_head_t wait_queue;
};

// 持有的锁信息
struct rb_held_lock {
    struct rb_mutex *mutex;
    int top_waiter_prio;
    struct rb_node node;
};

// 任务信息结构
struct rb_task_info {
    struct task_struct *task;
    struct rb_mutex *waiting_on;
    int original_prio;
    struct rb_root held_locks;
    struct hlist_node hnode;
};

#define RB_MUTEX_DEADLOCK_ERR -1002

// 核心接口
void rb_mutex_init(struct rb_mutex *mutex);
int rb_mutex_lock(struct rb_mutex *mutex);
void rb_mutex_unlock(struct rb_mutex *mutex);
int rb_mutex_trylock(struct rb_mutex *mutex);
int rb_mutex_lock_timeout(struct rb_mutex *mutex, unsigned long timeout_ms);
int rb_mutex_get_deadlock_count(void);

// 内部函数接口（如果.c需要多个文件共享）
struct rb_task_info *get_task_info(struct task_struct *task);
struct rb_task_info *ensure_task_info(struct task_struct *task);
struct rb_mutex_waiter *rb_mutex_top_waiter(struct rb_mutex *mutex);
void propagate_priority(struct task_struct *owner, int new_prio);
bool restore_priority(struct task_struct *task);
int get_effective_inherited_prio(struct rb_task_info *info);
void update_top_waiter_prio(struct rb_mutex *mutex);

#endif // RB_MUTEX_H
