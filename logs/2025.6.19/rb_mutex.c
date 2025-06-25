// rb_mutex.c
// 可编译进 Linux 内核的模块形式实现：红黑树 + 链式优先级继承互斥锁
#include "rb_mutex.h"  
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/rbtree.h>
#include <linux/string.h>
MODULE_LICENSE("GPL");
MODULE_AUTHOR("ldfnewbie");
MODULE_DESCRIPTION("RB-Tree Mutex with Chained Priority Inheritance (Kernel Module Demo)");

// ===== 修改处：新增字段支持链式继承 =====


struct task_node *alloc_task(const char *name, int prio)
{
    struct task_node *task = kmalloc(sizeof(*task), GFP_KERNEL);
    if (!task)
        return NULL;
    strncpy(task->name, name, sizeof(task->name));
    task->base_prio = task->curr_prio = prio;
    task->waiting_on = NULL; // ===== 修改处 =====
    return task;
}

void reset_priority(struct task_node *task)
{
    pr_info("%s resets to base priority %d\n", task->name, task->base_prio);
    task->curr_prio = task->base_prio;
}

// ===== 修改处：递归链式优先级继承 =====
void propagate_priority(struct task_node *task, int donated)
{
    if (donated > task->curr_prio) {
        pr_info("%s inherits priority %d\n", task->name, donated);
        task->curr_prio = donated;
        if (task->waiting_on && task->waiting_on->owner) {
            propagate_priority(task->waiting_on->owner, donated);
        }
    }
}

void rb_mutex_enqueue(struct rb_mutex *mutex, struct task_node *task)
{
    struct rb_node **new = &mutex->waiters.rb_node, *parent = NULL;
    struct task_node *entry;

    while (*new) {
        parent = *new;
        entry = rb_entry(parent, struct task_node, node);

        if (task->curr_prio > entry->curr_prio)
            new = &(*new)->rb_left;
        else
            new = &(*new)->rb_right;
    }

    rb_link_node(&task->node, parent, new);
    rb_insert_color(&task->node, &mutex->waiters);
}

 struct task_node *rb_mutex_top_waiter(struct rb_mutex *mutex)
{
    struct rb_node *node = mutex->waiters.rb_node;
    if (!node)
        return NULL;

    while (node->rb_left)
        node = node->rb_left;

    return rb_entry(node, struct task_node, node);
}

 void rb_mutex_dequeue(struct rb_mutex *mutex, struct task_node *task)
{
    rb_erase(&task->node, &mutex->waiters);
    task->node.rb_left = task->node.rb_right = NULL;
}

 void rb_mutex_lock(struct rb_mutex *mutex, struct task_node *task)
{
    if (!mutex->owner) {
        mutex->owner = task;
        pr_info("%s acquires mutex.\n", task->name);
    } else {
        pr_info("%s waits for mutex.\n", task->name);
        rb_mutex_enqueue(mutex, task);
        task->waiting_on = mutex; // ===== 修改处：标记等待对象 =====
        propagate_priority(mutex->owner, task->curr_prio); // ===== 修改处：递归传递 =====
    }
}

 void rb_mutex_unlock(struct rb_mutex *mutex)
{
    if (!mutex->owner)
        return;

    pr_info("%s releases mutex.\n", mutex->owner->name);

    if (!RB_EMPTY_ROOT(&mutex->waiters)) {
        struct task_node *next = rb_mutex_top_waiter(mutex);
        rb_mutex_dequeue(mutex, next);
        mutex->owner = next;
        next->waiting_on = NULL; // ===== 修改处：清除等待标志 =====
        pr_info("%s is now owner of mutex.\n", next->name);
    } else {
        reset_priority(mutex->owner);
        mutex->owner = NULL;
    }
}

 int __init rb_mutex_demo_init(void)
{
    struct rb_mutex mutex1 = {
        .owner = NULL,
        .waiters = RB_ROOT
    };
    struct rb_mutex mutex2 = {
        .owner = NULL,
        .waiters = RB_ROOT
    };

    // ===== 修改处：创建链式优先级场景 =====
    struct task_node *A = alloc_task("A", 60);
    struct task_node *B = alloc_task("B", 80);
    struct task_node *C = alloc_task("C", 90);

    rb_mutex_lock(&mutex1, A); // A获得mutex1
    rb_mutex_lock(&mutex2, B); // B获得mutex2
    rb_mutex_lock(&mutex1, B); // B等待mutex1 → A继承80
    rb_mutex_lock(&mutex2, C); // C等待mutex2 → B继承90 → A继承90（链式）

    rb_mutex_unlock(&mutex1);
    rb_mutex_unlock(&mutex2);

    kfree(A);
    kfree(B);
    kfree(C);

    return 0;
}

 void __exit rb_mutex_demo_exit(void)
{
    pr_info("rb_mutex module unloaded.\n");
}

module_init(rb_mutex_demo_init);
module_exit(rb_mutex_demo_exit);
