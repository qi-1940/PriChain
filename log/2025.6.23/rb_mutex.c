// rb_mutex.c
// Linux 内核模块：支持动态数量的互斥锁，通过链表管理，红黑树优先级继承机制，提供 /proc 状态查看和动态创建接口，并通过 kthread 模拟任务调度 + 动态指令控制行为

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/rbtree.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/spinlock.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ldfnewbie");
MODULE_DESCRIPTION("RB-Tree Mutex System with Dynamic Instances, Priority Inheritance, and Command Control");

#define PROC_NAME   "rb_mutex"
#define PROC_CTRL   "rb_mutex_ctrl"
#define PROC_TASKS  "rb_mutex_tasks"
#define PROC_TASK_CTRL "rb_mutex_task_ctrl"

// === 结构定义 ===
struct rb_mutex;

struct task_node {
    struct rb_node node;
    char name[16];
    int base_prio;
    int curr_prio;
    struct rb_mutex *waiting_on;
    struct list_head held_mutexes;
    struct task_struct *thread;
    struct list_head task_list;
};

struct rb_mutex {
    struct task_node *owner;
    struct rb_root waiters;
    struct list_head list;
    struct list_head held_entry;
    int id;
};

// === 全局链表管理 ===
static LIST_HEAD(mutex_list);
static LIST_HEAD(task_list);
static int next_mutex_id = 0;

static struct task_node *A = NULL, *B = NULL, *C = NULL;
// === 同步保护 ===
static DEFINE_SPINLOCK(task_list_lock);
static DEFINE_SPINLOCK(mutex_list_lock);

// 前向声明（供线程函数调用）
static ssize_t rb_mutex_proc_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos);

// === 红黑树操作 ===
static void rb_mutex_enqueue(struct rb_mutex *mutex, struct task_node *task)
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

static struct task_node *rb_mutex_top_waiter(struct rb_mutex *mutex)
{
    struct rb_node *node = mutex->waiters.rb_node;
    if (!node)
        return NULL;
    while (node->rb_left)
        node = node->rb_left;
    return rb_entry(node, struct task_node, node);
}

static void rb_mutex_dequeue(struct rb_mutex *mutex, struct task_node *task)
{
    rb_erase(&task->node, &mutex->waiters);
    task->node.rb_left = task->node.rb_right = NULL;
}

// === 优先级处理 ===
static void propagate_priority(struct task_node *task, int donated)
{
    if (donated > task->curr_prio) {
        pr_info("%s inherits priority %d\n", task->name, donated);
        task->curr_prio = donated;
        if (task->waiting_on && task->waiting_on->owner)
            propagate_priority(task->waiting_on->owner, donated);
    }
}

static void reset_priority(struct task_node *task)
{
    struct rb_mutex *held;
    struct list_head *pos;
    int max_prio = task->base_prio;

    list_for_each(pos, &task->held_mutexes) {
        held = list_entry(pos, struct rb_mutex, held_entry);
        if (!RB_EMPTY_ROOT(&held->waiters)) {
            struct task_node *top = rb_mutex_top_waiter(held);
            if (top && top->curr_prio > max_prio)
                max_prio = top->curr_prio;
        }
    }

    if (task->curr_prio != max_prio) {
        pr_info("%s resets to priority %d\n", task->name, max_prio);
        task->curr_prio = max_prio;
    }
}

// === 互斥锁操作 ===
static void rb_mutex_lock(struct rb_mutex *mutex, struct task_node *task)
{
    if (!mutex->owner) {
        mutex->owner = task;
        list_add_tail(&mutex->held_entry, &task->held_mutexes);
        pr_info("%s acquires mutex #%d\n", task->name, mutex->id);
    } else {
        pr_info("%s waits for mutex #%d\n", task->name, mutex->id);
        rb_mutex_enqueue(mutex, task);
        task->waiting_on = mutex;
        propagate_priority(mutex->owner, task->curr_prio);
    }
}

static void rb_mutex_unlock(struct rb_mutex *mutex)
{
    if (!mutex->owner)
        return;

    pr_info("%s releases mutex #%d\n", mutex->owner->name, mutex->id);
    list_del(&mutex->held_entry);

    if (!RB_EMPTY_ROOT(&mutex->waiters)) {
        struct task_node *next = rb_mutex_top_waiter(mutex);
        rb_mutex_dequeue(mutex, next);
        mutex->owner = next;
        next->waiting_on = NULL;
        list_add_tail(&mutex->held_entry, &next->held_mutexes);
        pr_info("%s is now owner of mutex #%d\n", next->name, mutex->id);
    } else {
        reset_priority(mutex->owner);
        mutex->owner = NULL;
    }
}

// === 初始化任务 ===
static struct task_node* alloc_task(const char *name, int prio, int (*fn)(void *))
{
    struct task_node *task = kmalloc(sizeof(*task), GFP_KERNEL);
    if (!task) return NULL;
    strncpy(task->name, name, sizeof(task->name));
    task->base_prio = task->curr_prio = prio;
    task->waiting_on = NULL;
    INIT_LIST_HEAD(&task->held_mutexes);
    INIT_LIST_HEAD(&task->task_list);
    if (fn) {
        task->thread = kthread_run(fn, task, "rb_mutex_%s", name);
        if (IS_ERR(task->thread)) {
            kfree(task);
            return NULL;
        }
    } else {
        task->thread = NULL;
    }
    list_add_tail(&task->task_list, &task_list);
    return task;
}

// === 示例线程行为 ===
static int task_fn_A(void *data)
{
    msleep(1000);
    rb_mutex_proc_write(NULL, "lock A 0", 9, NULL);
    msleep(2000);
    rb_mutex_proc_write(NULL, "unlock A 0", 11, NULL);
    return 0;
}

static int task_fn_B(void *data)
{
    msleep(500);
    rb_mutex_proc_write(NULL, "lock B 0", 9, NULL);
    msleep(3000);
    rb_mutex_proc_write(NULL, "unlock B 0", 11, NULL);
    return 0;
}

static int task_fn_C(void *data)
{
    msleep(1500);
    rb_mutex_proc_write(NULL, "lock C 0", 9, NULL);
    msleep(2000);
    rb_mutex_proc_write(NULL, "unlock C 0", 11, NULL);
    return 0;
}

// === 控制接口 ===
static struct task_node *find_task_by_name(const char *name)
{
    struct task_node *t;
    spin_lock(&task_list_lock);
    list_for_each_entry(t, &task_list, task_list) {
        if (strcmp(t->name, name) == 0) {
            spin_unlock(&task_list_lock);
            return t;
        }
    }
    spin_unlock(&task_list_lock);
    return NULL;
}


static ssize_t rb_mutex_proc_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    char kbuf[64];
    char cmd[8], tname[8];
    int id;
    if (count >= sizeof(kbuf)) return -EINVAL;
    if (copy_from_user(kbuf, buf, count)) return -EFAULT;
    kbuf[count] = '\0';

    if (sscanf(kbuf, "%7s %7s %d", cmd, tname, &id) >= 1) {
        if (strcmp(cmd, "create") == 0) {
            struct rb_mutex *m = kmalloc(sizeof(*m), GFP_KERNEL);
            if (!m) return -ENOMEM;
            m->owner = NULL;
            m->waiters = RB_ROOT;
            INIT_LIST_HEAD(&m->list);
            INIT_LIST_HEAD(&m->held_entry);
            spin_lock(&mutex_list_lock);
            m->id = next_mutex_id++;
            list_add_tail(&m->list, &mutex_list);
            spin_unlock(&mutex_list_lock);
            pr_info("Created mutex #%d\n", m->id);
        } else if (strcmp(cmd, "lock") == 0) {
            struct task_node *t = find_task_by_name(tname);
            struct rb_mutex *m;
            spin_lock(&mutex_list_lock);
            list_for_each_entry(m, &mutex_list, list) {
                if (m->id == id) {
                    rb_mutex_lock(m, t);
                    break;
                }
            }
            spin_unlock(&mutex_list_lock);
        } else if (strcmp(cmd, "unlock") == 0) {
            struct task_node *t = find_task_by_name(tname);
            struct rb_mutex *m;
            spin_lock(&mutex_list_lock);
            list_for_each_entry(m, &mutex_list, list) {
                if (m->id == id && m->owner == t) {
                    rb_mutex_unlock(m);
                    break;
                }
            }
            spin_unlock(&mutex_list_lock);
        }
    }
    return count;
}

// === 控制接口 ===
static ssize_t rb_mutex_task_ctrl_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    char kbuf[64];
    char name[16];
    int prio;
    if (count >= sizeof(kbuf)) return -EINVAL;
    if (copy_from_user(kbuf, buf, count)) return -EFAULT;
    kbuf[count] = '\0';

    if (sscanf(kbuf, "%15s %d", name, &prio) == 2) {
        struct task_node *task = alloc_task(name, prio, NULL);
        if (!task) return -ENOMEM;
        spin_lock(&task_list_lock);
        list_add_tail(&task->task_list, &task_list);
        spin_unlock(&task_list_lock);
        pr_info("Created task %s with prio %d\n", name, prio);
    }
    return count;
}

static const struct file_operations rb_mutex_task_ctrl_fops = {
    .owner = THIS_MODULE,
    .write = rb_mutex_task_ctrl_write,
};

// === /proc 展示 ===
static int rb_mutex_proc_show(struct seq_file *m, void *v)
{
    struct rb_mutex *mutex;
    struct rb_node *node;
    struct task_node *task;

    list_for_each_entry(mutex, &mutex_list, list) {
        seq_printf(m, "\nMutex #%d:\n", mutex->id);
        seq_printf(m, "  Owner: %s (prio=%d)\n",
                   mutex->owner ? mutex->owner->name : "None",
                   mutex->owner ? mutex->owner->curr_prio : -1);
        seq_puts(m, "  Waiters:\n");
        for (node = rb_first(&mutex->waiters); node; node = rb_next(node)) {
            task = rb_entry(node, struct task_node, node);
            seq_printf(m, "   - %s (curr_prio=%d, base_prio=%d)\n",
                       task->name, task->curr_prio, task->base_prio);
        }
    }
    return 0;
}

static int rb_mutex_tasks_show(struct seq_file *m, void *v)
{
    struct task_node *task;
    list_for_each_entry(task, &task_list, task_list) {
        seq_printf(m, "Task %s: base_prio=%d, curr_prio=%d\n",
                   task->name, task->base_prio, task->curr_prio);
    }
    return 0;
}

static int rb_mutex_tasks_open(struct inode *inode, struct file *file)
{
    return single_open(file, rb_mutex_tasks_show, NULL);
}

static const struct file_operations rb_mutex_tasks_fops = {
    .owner = THIS_MODULE,
    .open = rb_mutex_tasks_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static int rb_mutex_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, rb_mutex_proc_show, NULL);
}

static const struct file_operations rb_mutex_proc_fops = {
    .owner = THIS_MODULE,
    .open = rb_mutex_proc_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static const struct file_operations rb_mutex_proc_ctrl_fops = {
    .owner = THIS_MODULE,
    .write = rb_mutex_proc_write,
};
static int __init rb_mutex_demo_init(void)
{
    A = alloc_task("A", 60, task_fn_A);
    B = alloc_task("B", 80, task_fn_B);
    C = alloc_task("C", 90, task_fn_C);

    proc_create(PROC_NAME, 0, NULL, &rb_mutex_proc_fops);
    proc_create(PROC_CTRL, 0666, NULL, &rb_mutex_proc_ctrl_fops);
    proc_create(PROC_TASKS, 0, NULL, &rb_mutex_tasks_fops);
    proc_create(PROC_TASK_CTRL, 0666, NULL, &rb_mutex_task_ctrl_fops);
    pr_info("rb_mutex module loaded\n");
    return 0;
}

static void __exit rb_mutex_demo_exit(void)
{
    struct rb_mutex *m, *tmp;
    struct task_node *t, *ttmp;

    remove_proc_entry(PROC_NAME, NULL);
    remove_proc_entry(PROC_CTRL, NULL);
    remove_proc_entry(PROC_TASKS, NULL);
    remove_proc_entry(PROC_TASK_CTRL, NULL);

    list_for_each_entry_safe(m, tmp, &mutex_list, list) {
        list_del(&m->list);
        kfree(m);
    }

    list_for_each_entry_safe(t, ttmp, &task_list, task_list) {
        list_del(&t->task_list);
        kfree(t);
    }

    pr_info("rb_mutex module unloaded\n");
}

module_init(rb_mutex_demo_init);
module_exit(rb_mutex_demo_exit);
