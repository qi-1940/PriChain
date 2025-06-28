#ifndef RB_RWSEM_H
#define RB_RWSEM_H

#include "rb_mutex.h"

struct rb_rwsem {
    struct rb_mutex lock;      // 保护内部状态
    struct rb_mutex wr_lock;   // 写锁（独占）
    int readers;               // 当前持有读锁的线程数
};

void rb_rwsem_init(struct rb_rwsem *sem);
void rb_rwsem_down_read(struct rb_rwsem *sem);
void rb_rwsem_up_read(struct rb_rwsem *sem);
void rb_rwsem_down_write(struct rb_rwsem *sem);
void rb_rwsem_up_write(struct rb_rwsem *sem);

#endif // RB_RWSEM_H 