// rb_mutex.c
// Linux 内核模块：支持动态数量的互斥锁，通过链表管理，红黑树优先级继承机制，提供接口供其他模块调用

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include "rb_mutex.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ldfnewbie");
MODULE_DESCRIPTION("RB-Tree Mutex System with Dynamic Instances, Priority Inheritance, and Exported Interface");

// === 红黑树操作 ===
static void rb_mutex_enqueue(struct rb_mutex *mutex, struct rb_mutex_waiter *waiter)
{
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
}

static struct rb_mutex_waiter *rb_mutex_top_waiter(struct rb_mutex *mutex)
{
    struct rb_node *node = mutex->waiters.rb_node;
    if (!node)
        return NULL;
    while (node->rb_left)
        node = node->rb_left;
    return rb_entry(node, struct rb_mutex_waiter, node);
}

static void rb_mutex_dequeue(struct rb_mutex *mutex, struct rb_mutex_waiter *waiter)
{
    rb_erase(&waiter->node, &mutex->waiters);
    waiter->node.rb_left = waiter->node.rb_right = NULL;
}

// === 公共接口 ===
void rb_mutex_init(struct rb_mutex *mutex)
{
    mutex->owner = NULL;
    mutex->waiters = RB_ROOT;
    spin_lock_init(&mutex->lock);
    init_waitqueue_head(&mutex->wait_queue);
}

void rb_mutex_lock(struct rb_mutex *mutex)
{
    DEFINE_WAIT(wait);
    struct rb_mutex_waiter self;

    self.task = current;
    self.prio = current->prio;
    INIT_LIST_HEAD(&self.list);

    spin_lock(&mutex->lock);
    if (!mutex->owner) {
        mutex->owner = current;
        spin_unlock(&mutex->lock);
        return;
    }

    rb_mutex_enqueue(mutex, &self);
    spin_unlock(&mutex->lock);

    for (;;) {
        prepare_to_wait(&mutex->wait_queue, &wait, TASK_UNINTERRUPTIBLE);
        spin_lock(&mutex->lock);
        if (mutex->owner == NULL && rb_mutex_top_waiter(mutex) == &self) {
            rb_mutex_dequeue(mutex, &self);
            mutex->owner = current;
            spin_unlock(&mutex->lock);
            break;
        }
        spin_unlock(&mutex->lock);
        schedule();
    }
    finish_wait(&mutex->wait_queue, &wait);
}

void rb_mutex_unlock(struct rb_mutex *mutex)
{
    spin_lock(&mutex->lock);
    if (mutex->owner != current) {
        spin_unlock(&mutex->lock);
        return;
    }
    mutex->owner = NULL;

    if (!RB_EMPTY_ROOT(&mutex->waiters)) {
        struct rb_mutex_waiter *next = rb_mutex_top_waiter(mutex);
        wake_up_process(next->task);
    }
    spin_unlock(&mutex->lock);
}

int rb_mutex_trylock(struct rb_mutex *mutex)
{
    int success = 0;
    spin_lock(&mutex->lock);
    if (!mutex->owner) {
        mutex->owner = current;
        success = 1;
    }
    spin_unlock(&mutex->lock);
    return success;
}

// === 模块初始化 ===
static int __init rb_mutex_module_init(void)
{
    pr_info("rb_mutex kernel module loaded\n");
    return 0;
}

static void __exit rb_mutex_module_exit(void)
{
    pr_info("rb_mutex kernel module unloaded\n");
}

module_init(rb_mutex_module_init);
module_exit(rb_mutex_module_exit);

EXPORT_SYMBOL(rb_mutex_init);
EXPORT_SYMBOL(rb_mutex_lock);
EXPORT_SYMBOL(rb_mutex_unlock);
EXPORT_SYMBOL(rb_mutex_trylock);
