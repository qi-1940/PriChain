#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/delay.h>
#include <linux/cpumask.h>

struct sched_param {
    int sched_priority;
};

static struct mutex test_lock;
static struct task_struct *task_l, *task_h, *task_m;

static void pin_to_cpu(int cpu_id) {
    struct cpumask mask;
    cpumask_clear(&mask);
    cpumask_set_cpu(cpu_id, &mask); // 绑定到 cpu_id 指定的核心
    set_cpus_allowed_ptr(current, &mask); // 限制当前线程
    printk(KERN_INFO "Pinned to CPU %d\n", cpu_id);
}

int low_prio_thread(void *data)
{
    struct sched_param sp;
    int i;
    sp.sched_priority = 10;
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);

    pr_alert("mutex L: START, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    
    mutex_lock(&test_lock);
    pr_alert("mutex L: GOT mutex, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    for (i = 0; i < 4; i++) {
        pr_alert("mutex L: time=%lu, current prio: %d, normal_prio: %d\n",
                jiffies, current->prio, current->normal_prio);
        msleep(1000);
    }
    
    mutex_unlock(&test_lock);
    pr_alert("mutex L: END, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    return 0;
}

int mid_prio_thread(void *data)
{
    ssleep(1);// 确保L和H先执行
    struct sched_param sp;
    int i;
    sp.sched_priority = 15;
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    pr_alert("mutex M: START, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);    
    for (i = 0; i < 10; i++) {
        pr_alert("mutex M: time=%lu, prio: %d, normal_prio: %d\n", jiffies, current->prio, current->normal_prio);
        msleep(1);
    }
    pr_alert("mutex M: END, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    return 0;
}

int high_prio_thread(void *data)
{
    ssleep(0.5); // 确保L先获得锁
    struct sched_param sp;
    sp.sched_priority = 20;
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    pr_alert("mutex H: START, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    
    mutex_lock(&test_lock);
    pr_alert("mutex H: GOT mutex, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    mutex_unlock(&test_lock);
    pr_alert("mutex H: END, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    return 0;
}

static int __init mutex_test_init(void)
{
    pin_to_cpu(0); // 绑定到 CPU 0
    pr_info("mutex test module loaded\n");

    mutex_init(&test_lock);

    task_l = kthread_run(low_prio_thread, NULL, "low_prio_thread");
    task_h = kthread_run(high_prio_thread, NULL, "high_prio_thread");
    task_m = kthread_run(mid_prio_thread, NULL, "mid_prio_thread");
    

    return 0;
}

static void __exit mutex_test_exit(void)
{
    
    pr_info("mutex test module unloaded\n");
}

module_init(mutex_test_init);
module_exit(mutex_test_exit);
MODULE_LICENSE("GPL"); 