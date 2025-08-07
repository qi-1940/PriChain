/*
 * 在不改变内核task_struct结构体的前提下，
 * 通过独立的辅助结构体（rb_task_info 记录任务锁信息，rb_held_lock 记录任务持有的锁，等待者和锁视角）
 * 这样更易于理解和维护，不依赖内核调度核心结构体的修改，扩展性和兼容性更好
*/
#ifndef RB_MUTEX_H
#define RB_MUTEX_H

#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/rbtree.h>
#include <linux/sched.h>
#include <linux/hashtable.h>

// 互斥锁等待者->本质不是任务本身，而是作为节点以等待者视角指向任务
// 记录哪个任务在等锁，它的优先级，以及在红黑树中的位置
struct rb_mutex_waiter {
    struct task_struct *task;
    int prio;
    struct rb_node node;
    struct list_head list;
};

// 互斥锁->本质是锁
// 记录当前 owner、所有等待者、并发保护用的自旋锁、内核等待队列等
struct rb_mutex {
    struct task_struct *owner;
    struct rb_root waiters; // 等待该锁的所有任务（红黑树，按优先级排序）
    spinlock_t lock; // 保证锁元数据本身并发访问，如多个任务尝试加锁
    wait_queue_head_t wait_queue; // 配合调度器
};

// 持有的锁信息
// 用于任务持有多把锁时，记录每把锁的最高等待者优先级，便于优先级合并和恢复
// 每个任务有一个 held_locks 红黑树，里面每个节点就是一个 rb_held_lock，指向具体的锁
struct rb_held_lock {
    struct rb_mutex *mutex;
    int top_waiter_prio;
    struct rb_node node;
};

// 任务信息结构->与锁有关，管理和锁相关的状态
// 记录任务本身、正在等待的锁、原始优先级、持有的所有锁、哈希表节点、递归继承链
struct rb_task_info {
    struct task_struct *task;
    struct rb_mutex *waiting_on;
    int original_prio;
    struct rb_root held_locks;
    struct hlist_node hnode;
    struct rb_mutex *blocked_lock; // 当前任务阻塞在哪个锁上
};

#define RB_MUTEX_DEADLOCK_ERR -1002

// 核心接口
void rb_mutex_init(struct rb_mutex *mutex);
int rb_mutex_lock(struct rb_mutex *mutex);
void rb_mutex_unlock(struct rb_mutex *mutex);
int rb_mutex_trylock(struct rb_mutex *mutex);
int rb_mutex_lock_timeout(struct rb_mutex *mutex, unsigned long timeout_ms);
int rb_mutex_get_deadlock_count(void);

// 内部函数接口
struct rb_task_info *get_task_info(struct task_struct *task);
struct rb_task_info *ensure_task_info(struct task_struct *task);
struct rb_mutex_waiter *rb_mutex_top_waiter(struct rb_mutex *mutex);
void propagate_priority(struct task_struct *owner, int new_prio);
bool restore_priority(struct task_struct *task);
int get_effective_inherited_prio(struct rb_task_info *info);
void update_top_waiter_prio(struct rb_mutex *mutex);

#endif
