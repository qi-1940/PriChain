#include <linux/rb_rwmutex.h>
#include <linux/module.h>

void rb_rwmutex_init(struct rb_rwmutex *rw) {
    rb_mutex_init(&rw->lock);
    rb_mutex_init(&rw->wr_lock);
    rw->readers = 0;
}

void rb_rwmutex_read_lock(struct rb_rwmutex *rw) {
    rb_mutex_lock(&rw->lock);
    if (rw->readers == 0) {
        // 第一个读者需要获取写锁，防止写者进入
        rb_mutex_lock(&rw->wr_lock);
    }
    rw->readers++;
    rb_mutex_unlock(&rw->lock);
}

void rb_rwmutex_read_unlock(struct rb_rwmutex *rw) {
    rb_mutex_lock(&rw->lock);
    rw->readers--;
    if (rw->readers == 0) {
        // 最后一个读者释放写锁
        rb_mutex_unlock(&rw->wr_lock);
    }
    rb_mutex_unlock(&rw->lock);
}

void rb_rwmutex_write_lock(struct rb_rwmutex *rw) {
    rb_mutex_lock(&rw->wr_lock);
}

void rb_rwmutex_write_unlock(struct rb_rwmutex *rw) {
    rb_mutex_unlock(&rw->wr_lock);
} 

EXPORT_SYMBOL(rb_rwmutex_init);
EXPORT_SYMBOL(rb_rwmutex_read_lock);
EXPORT_SYMBOL(rb_rwmutex_read_unlock);
EXPORT_SYMBOL(rb_rwmutex_write_lock);
EXPORT_SYMBOL(rb_rwmutex_write_unlock);