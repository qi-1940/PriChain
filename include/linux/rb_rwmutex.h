#ifndef RB_RWMUTEX_H
#define RB_RWMUTEX_H

#include "rb_mutex.h"

struct rb_rwmutex {
    struct rb_mutex lock;      // 保护内部状态
    struct rb_mutex wr_lock;   // 写锁（独占）
    int readers;               // 当前持有读锁的线程数
};

void rb_rwmutex_init(struct rb_rwmutex *rw);
void rb_rwmutex_read_lock(struct rb_rwmutex *rw);
void rb_rwmutex_read_unlock(struct rb_rwmutex *rw);
void rb_rwmutex_write_lock(struct rb_rwmutex *rw);
void rb_rwmutex_write_unlock(struct rb_rwmutex *rw);

#endif // RB_RWMUTEX_H 