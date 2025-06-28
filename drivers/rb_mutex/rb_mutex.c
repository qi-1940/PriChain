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
#include "rb_mutex.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ldfnewbie + ChatGPT");
MODULE_DESCRIPTION("RB-Tree Mutex System with Dynamic Instances, Priority Inheritance, and Exported Interface");

#define RB_TASK_TABLE_BITS 6
static DEFINE_HASHTABLE(rb_task_table, RB_TASK_TABLE_BITS);
static DEFINE_SPINLOCK(rb_task_table_lock);

// 死锁统计
static atomic_t rb_mutex_deadlock_count = ATOMIC_INIT(0);

struct rb_held_lock *find_held_lock(struct rb_root *root, struct rb_mutex *mutex) {
    struct rb_node *node = root->rb_node;
    while (node) {
        struct rb_held_lock *hl = rb_entry(node, struct rb_held_lock, node);
        if (mutex < hl->mutex)
            node = node->rb_left;
        else if (mutex > hl->mutex)
            node = node->rb_right;
        else
            return hl;
    }
    return NULL;
}

void insert_held_lock(struct rb_task_info *info, struct rb_mutex *mutex, int top_prio) {
    struct rb_node **new = &info->held_locks.rb_node, *parent = NULL;
    struct rb_held_lock *hl;

    hl = kmalloc(sizeof(*hl), GFP_ATOMIC);
    if (!hl) return;

    hl->mutex = mutex;
    hl->top_waiter_prio = top_prio;

    while (*new) {
        struct rb_held_lock *entry = rb_entry(*new, struct rb_held_lock, node);
        parent = *new;
        if (mutex < entry->mutex)
            new = &(*new)->rb_left;
        else
            new = &(*new)->rb_right;
    }
    rb_link_node(&hl->node, parent, new);
    rb_insert_color(&hl->node, &info->held_locks);
}

void remove_held_lock(struct rb_task_info *info, struct rb_mutex *mutex) {
    struct rb_held_lock *hl = find_held_lock(&info->held_locks, mutex);
    if (!hl) return;
    rb_erase(&hl->node, &info->held_locks);
    kfree(hl);
}

int get_effective_inherited_prio(struct rb_task_info *info) {
    struct rb_node *node = rb_first(&info->held_locks);
    int max_prio = -1;
    while (node) {
        struct rb_held_lock *hl = rb_entry(node, struct rb_held_lock, node);
        if (hl->top_waiter_prio > max_prio)
            max_prio = hl->top_waiter_prio;
        node = rb_next(node);
    }
    return max_prio;
}

void update_top_waiter_prio(struct rb_mutex *mutex) {
    struct rb_mutex_waiter *top = rb_mutex_top_waiter(mutex);
    int top_prio = top ? top->prio : -1;

    if (!mutex->owner) return;

    struct rb_task_info *owner_info = get_task_info(mutex->owner);
    if (!owner_info) return;

    struct rb_held_lock *hl = find_held_lock(&owner_info->held_locks, mutex);
    if (hl) {
        hl->top_waiter_prio = top_prio;
    }
}

struct rb_task_info *get_task_info(struct task_struct *task) {
    struct rb_task_info *info;
    hash_for_each_possible(rb_task_table, info, hnode, (unsigned long)task) {
        if (info->task == task)
            return info;
    }
    return NULL;
}

struct rb_task_info *ensure_task_info(struct task_struct *task) {
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
            info->held_locks = RB_ROOT;
            hash_add(rb_task_table, &info->hnode, (unsigned long)task);
        }
    }
    spin_unlock_irqrestore(&rb_task_table_lock, flags);
    return info;
}

void rb_mutex_enqueue(struct rb_mutex *mutex, struct rb_mutex_waiter *waiter) {
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
    update_top_waiter_prio(mutex);
}

struct rb_mutex_waiter *rb_mutex_top_waiter(struct rb_mutex *mutex) {
    struct rb_node *node = mutex->waiters.rb_node;
    if (!node)
        return NULL;
    while (node->rb_left)
        node = node->rb_left;
    return rb_entry(node, struct rb_mutex_waiter, node);
}

void rb_mutex_dequeue(struct rb_mutex *mutex, struct rb_mutex_waiter *waiter) {
    rb_erase(&waiter->node, &mutex->waiters);
    waiter->node.rb_left = waiter->node.rb_right = NULL;
    update_top_waiter_prio(mutex);
}

void propagate_priority(struct task_struct *owner, int new_prio) {
    struct rb_task_info *info = ensure_task_info(owner);
    if (!info) return;

    if (info->original_prio < 0)
        info->original_prio = owner->prio;

    int effective = get_effective_inherited_prio(info);
    if (effective < new_prio)
        effective = new_prio;

    if (effective > 0 && owner->prio > effective) {
        owner->prio = effective;
        pr_info("[rb_mutex] %s inherits prio %d\n", owner->comm, effective);

        // 如果 owner 被阻塞，唤醒它
        wake_up_process(owner);
    }

    // 向上传播优先级
    if (info->waiting_on && info->waiting_on->owner) {
        propagate_priority(info->waiting_on->owner, effective);
    }
}

bool task_still_blocking_others(struct task_struct *task) {
    struct rb_task_info *info;
    bool blocking = false;
    unsigned long flags;
    int bkt;

    spin_lock_irqsave(&rb_task_table_lock, flags);
    hash_for_each(rb_task_table, bkt, info, hnode) {
        if (info->waiting_on && info->waiting_on->owner == task) {
            blocking = true;
            break;
        }
    }
    spin_unlock_irqrestore(&rb_task_table_lock, flags);

    return blocking;
}
bool restore_priority(struct task_struct *task) {
    struct rb_task_info *info;
    int inherited;
    struct rb_node *node;
    bool changed = false;


    info = get_task_info(task);
    if (!info)
        return false;

    inherited = get_effective_inherited_prio(info);
    if (inherited >= 0 && inherited < task->prio) {
        task->prio = inherited;
        pr_info("[rb_mutex] downgrade prio for %s to inherited %d\n", task->comm, inherited);
        changed =true;
    } else if (info->original_prio >= 0 && task->prio != info->original_prio) {
        pr_info("[rb_mutex] restore prio for %s to %d\n", task->comm, info->original_prio);
        task->prio = info->original_prio;
        info->original_prio = -1;
        changed=true;
    }

    // 遍历当前任务持有的所有互斥锁，唤醒各自的最高优先级等待者
    node = rb_first(&info->held_locks);
    while (node) {
        struct rb_held_lock *hl = rb_entry(node, struct rb_held_lock, node);
        struct rb_mutex *mutex = hl->mutex;

        if (!RB_EMPTY_ROOT(&mutex->waiters)) {
            struct rb_mutex_waiter *next = rb_mutex_top_waiter(mutex);
            if (next && next->task)
                wake_up_process(next->task);
        }

        node = rb_next(node);
    }
    return changed;

}

void rb_mutex_init(struct rb_mutex *mutex) {
    mutex->owner = NULL;
    mutex->waiters = RB_ROOT;
    spin_lock_init(&mutex->lock);
    init_waitqueue_head(&mutex->wait_queue);
}

// 死锁检测：沿owner->waiting_on链查找是否有环路，并打印链路
static int rb_mutex_detect_deadlock(struct rb_mutex *mutex, struct task_struct *self) {
    struct rb_mutex *cur_mutex = mutex;
    struct task_struct *owner;
    struct rb_task_info *owner_info;
    int depth = 0;
    char chain[512] = {0};
    int offset = 0;
    char mermaid[1024] = "graph TD; ";
    int moffset = strlen(mermaid);
    char prev_node[64];
    snprintf(prev_node, sizeof(prev_node), "T%d", self->pid);
    moffset += snprintf(mermaid+moffset, sizeof(mermaid)-moffset, "%s[\"%s(pid=%d)\"]", prev_node, self->comm, self->pid);
    offset += snprintf(chain+offset, sizeof(chain)-offset, "%s(pid=%d)", self->comm, self->pid);
    while (cur_mutex && (owner = cur_mutex->owner)) {
        owner_info = get_task_info(owner);
        offset += snprintf(chain+offset, sizeof(chain)-offset, " -> [lock %p] -> %s(pid=%d)", cur_mutex, owner->comm, owner->pid);
        char lock_node[64], owner_node[64];
        snprintf(lock_node, sizeof(lock_node), "L%p", cur_mutex);
        snprintf(owner_node, sizeof(owner_node), "T%d", owner->pid);
        moffset += snprintf(mermaid+moffset, sizeof(mermaid)-moffset, "; %s-->|lock %p|%s; %s[\"lock %p\"]", prev_node, cur_mutex, owner_node, lock_node, cur_mutex);
        moffset += snprintf(mermaid+moffset, sizeof(mermaid)-moffset, "; %s[\"%s(pid=%d)\"]", owner_node, owner->comm, owner->pid);
        strncpy(prev_node, owner_node, sizeof(prev_node));
        if (owner == self) {
            atomic_inc(&rb_mutex_deadlock_count);
            pr_warn("[rb_mutex] Deadlock detected: chain: %s\n", chain);
            pr_warn("[rb_mutex] Deadlock chain (Mermaid):\n%s\n", mermaid);
            return 1;
        }
        if (!owner_info) break;
        cur_mutex = owner_info->waiting_on;
        if (++depth > 64) {
            pr_warn("[rb_mutex] Deadlock detection: chain too deep, possible bug\n");
            break;
        }
    }
    return 0;
}

int rb_mutex_lock(struct rb_mutex *mutex) {
    DEFINE_WAIT(wait);
    struct rb_mutex_waiter self;
    struct rb_task_info *info = ensure_task_info(current);

    // --- 死锁检测 ---
    spin_lock(&mutex->lock);
    if (rb_mutex_detect_deadlock(mutex, current)) {
        spin_unlock(&mutex->lock);
        return RB_MUTEX_DEADLOCK_ERR;
    }
    // ... existing code ...
    self.task = current;
    self.prio = current->prio;
    INIT_LIST_HEAD(&self.list);

    if (!mutex->owner) {
        mutex->owner = current;
        if (info) {
            info->waiting_on = NULL;
            insert_held_lock(info, mutex, rb_mutex_top_waiter(mutex) ? rb_mutex_top_waiter(mutex)->prio : -1);
        }
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
            if (info) {
                info->waiting_on = NULL;
                insert_held_lock(info, mutex, rb_mutex_top_waiter(mutex) ? rb_mutex_top_waiter(mutex)->prio : -1);
            }
            spin_unlock(&mutex->lock);
            break;
        }
        spin_unlock(&mutex->lock);
        schedule();
    }
    finish_wait(&mutex->wait_queue, &wait);
    return 0;
}

void rb_mutex_unlock(struct rb_mutex *mutex) {
    struct rb_task_info *info = get_task_info(current);
    struct rb_mutex_waiter *next = NULL;
    bool need_restore = false;

    spin_lock(&mutex->lock);
    if (mutex->owner != current) {
        spin_unlock(&mutex->lock);
        return;
    }
    mutex->owner = NULL;

    if (info) {
        remove_held_lock(info, mutex);
        if (!task_still_blocking_others(current))
            need_restore = true;
    }

    if (!RB_EMPTY_ROOT(&mutex->waiters)) {
        next = rb_mutex_top_waiter(mutex);
        if (next && next->task)
            wake_up_process(next->task);
    }

    spin_unlock(&mutex->lock);

    if (need_restore&&restore_priority(current)){

    
         set_current_state(TASK_UNINTERRUPTIBLE);
        schedule_timeout(usecs_to_jiffies(20));
        __set_current_state(TASK_RUNNING);
    }
}


int rb_mutex_trylock(struct rb_mutex *mutex) {
    int success = 0;
    struct rb_task_info *info = ensure_task_info(current);

    spin_lock(&mutex->lock);
    if (!mutex->owner) {
        mutex->owner = current;
        if (info) {
            info->waiting_on = NULL;
            insert_held_lock(info, mutex, rb_mutex_top_waiter(mutex) ? rb_mutex_top_waiter(mutex)->prio : -1);
        }
        success = 1;
    }
    spin_unlock(&mutex->lock);
    return success;
}

int rb_mutex_lock_timeout(struct rb_mutex *mutex, unsigned long timeout_ms) {
    DEFINE_WAIT(wait);
    struct rb_mutex_waiter self;
    struct rb_task_info *info = ensure_task_info(current);
    unsigned long deadline = jiffies + msecs_to_jiffies(timeout_ms);

    // --- 死锁检测 ---
    spin_lock(&mutex->lock);
    if (rb_mutex_detect_deadlock(mutex, current)) {
        spin_unlock(&mutex->lock);
        return RB_MUTEX_DEADLOCK_ERR;
    }
    // ... existing code ...
    if (mutex->owner == current) {
        pr_warn("[rb_mutex] Deadlock detected: %s tried to re-lock\n", current->comm);
        spin_unlock(&mutex->lock);
        return -EDEADLK;
    }
    self.task = current;
    self.prio = current->prio;
    INIT_LIST_HEAD(&self.list);

    if (!mutex->owner) {
        mutex->owner = current;
        if (info) {
            info->waiting_on = NULL;
            insert_held_lock(info, mutex, rb_mutex_top_waiter(mutex) ? rb_mutex_top_waiter(mutex)->prio : -1);
        }
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
            if (info) {
                info->waiting_on = NULL;
                insert_held_lock(info, mutex, rb_mutex_top_waiter(mutex) ? rb_mutex_top_waiter(mutex)->prio : -1);
            }
            spin_unlock(&mutex->lock);
            break;
        }
        spin_unlock(&mutex->lock);

        if (time_after(jiffies, deadline)) {
            spin_lock(&mutex->lock);
            rb_mutex_dequeue(mutex, &self);
            spin_unlock(&mutex->lock);

            if (info) info->waiting_on = NULL;
            finish_wait(&mutex->wait_queue, &wait);
            return -ETIMEDOUT;
        }

        schedule_timeout(msecs_to_jiffies(10));
    }

    finish_wait(&mutex->wait_queue, &wait);
    return 0;
}

int rb_mutex_get_deadlock_count(void) {
    return atomic_read(&rb_mutex_deadlock_count);
}

static int __init rb_mutex_module_init(void) {
    pr_info("rb_mutex kernel module loaded\n");
    return 0;
}

static void __exit rb_mutex_module_exit(void) {
    pr_info("rb_mutex kernel module unloaded\n");
}

module_init(rb_mutex_module_init);
module_exit(rb_mutex_module_exit);

EXPORT_SYMBOL(rb_mutex_init);
EXPORT_SYMBOL(rb_mutex_lock);
EXPORT_SYMBOL(rb_mutex_unlock);
EXPORT_SYMBOL(rb_mutex_trylock);
EXPORT_SYMBOL(rb_mutex_lock_timeout);
EXPORT_SYMBOL(rb_mutex_get_deadlock_count);
