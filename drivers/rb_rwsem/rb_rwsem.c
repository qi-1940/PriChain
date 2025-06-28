#include <linux/rb_rwsem.h>
#include <linux/module.h>

void rb_rwsem_init(struct rb_rwsem *sem) {
    rb_mutex_init(&sem->lock);
    rb_mutex_init(&sem->wr_lock);
    sem->readers = 0;
}

void rb_rwsem_down_read(struct rb_rwsem *sem) {
    rb_mutex_lock(&sem->lock);
    if (sem->readers == 0) {
        rb_mutex_lock(&sem->wr_lock);
    }
    sem->readers++;
    rb_mutex_unlock(&sem->lock);
}

void rb_rwsem_up_read(struct rb_rwsem *sem) {
    rb_mutex_lock(&sem->lock);
    sem->readers--;
    if (sem->readers == 0) {
        rb_mutex_unlock(&sem->wr_lock);
    }
    rb_mutex_unlock(&sem->lock);
}

void rb_rwsem_down_write(struct rb_rwsem *sem) {
    rb_mutex_lock(&sem->wr_lock);
}

void rb_rwsem_up_write(struct rb_rwsem *sem) {
    rb_mutex_unlock(&sem->wr_lock);
}

EXPORT_SYMBOL(rb_rwsem_init);
EXPORT_SYMBOL(rb_rwsem_down_read);
EXPORT_SYMBOL(rb_rwsem_up_read);
EXPORT_SYMBOL(rb_rwsem_down_write);
EXPORT_SYMBOL(rb_rwsem_up_write); 