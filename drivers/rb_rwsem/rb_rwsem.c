#include "../include/linux/rb_rwsem.h"
#include <linux/printk.h>
#include <linux/ktime.h>
#include <linux/sched/rt.h>
#include <linux/sched.h>

void rb_rwsem_init(struct rb_rwsem *sem) {
    rb_mutex_init(&sem->lock);
    rb_mutex_init(&sem->wr_lock);
    sem->readers = 0;
}

void rb_rwsem_down_read(struct rb_rwsem *sem) {
    pr_alert("[rb_rwsem] %s (pid=%d) before down_read, readers=%d, prio=%d, normal_prio=%d\n",
             current->comm, current->pid, sem->readers, current->prio, current->normal_prio);
    rb_mutex_lock(&sem->lock);
    if (sem->readers == 0) {
        pr_alert("[rb_rwsem] %s (pid=%d) first reader, acquiring wr_lock\n", current->comm, current->pid);
        rb_mutex_lock(&sem->wr_lock);
        pr_alert("[rb_rwsem] %s (pid=%d) acquired wr_lock for readers, prio=%d, normal_prio=%d\n",
                 current->comm, current->pid, current->prio, current->normal_prio);
    }
    sem->readers++;
    pr_alert("[rb_rwsem] %s (pid=%d) now holds read, readers=%d, prio=%d, normal_prio=%d\n",
             current->comm, current->pid, sem->readers, current->prio, current->normal_prio);
    rb_mutex_unlock(&sem->lock);
}

void rb_rwsem_up_read(struct rb_rwsem *sem) {
    pr_alert("[rb_rwsem] %s (pid=%d) before up_read, readers=%d, prio=%d, normal_prio=%d\n",
             current->comm, current->pid, sem->readers, current->prio, current->normal_prio);
    rb_mutex_lock(&sem->lock);
    sem->readers--;
    pr_alert("[rb_rwsem] %s (pid=%d) after up_read, readers=%d, prio=%d, normal_prio=%d\n",
             current->comm, current->pid, sem->readers, current->prio, current->normal_prio);
    if (sem->readers == 0) {
        pr_alert("[rb_rwsem] %s (pid=%d) last reader, releasing wr_lock\n", current->comm, current->pid);
        rb_mutex_unlock(&sem->wr_lock);
    }
    rb_mutex_unlock(&sem->lock);
}

void rb_rwsem_down_write(struct rb_rwsem *sem) {
    pr_alert("[rb_rwsem] %s (pid=%d) attempting down_write, prio=%d, normal_prio=%d\n",
             current->comm, current->pid, current->prio, current->normal_prio);
    rb_mutex_lock(&sem->wr_lock);
    pr_alert("[rb_rwsem] %s (pid=%d) got write lock, prio=%d, normal_prio=%d\n",
             current->comm, current->pid, current->prio, current->normal_prio);
}

void rb_rwsem_up_write(struct rb_rwsem *sem) {
    pr_alert("[rb_rwsem] %s (pid=%d) releasing write lock, prio=%d, normal_prio=%d\n",
             current->comm, current->pid, current->prio, current->normal_prio);
    rb_mutex_unlock(&sem->wr_lock);
}

EXPORT_SYMBOL(rb_rwsem_init);
EXPORT_SYMBOL(rb_rwsem_down_read);
EXPORT_SYMBOL(rb_rwsem_up_read);
EXPORT_SYMBOL(rb_rwsem_down_write);
EXPORT_SYMBOL(rb_rwsem_up_write); 