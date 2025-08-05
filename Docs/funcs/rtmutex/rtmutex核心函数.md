## rt_mutex_lock()

```c
void __sched rt_mutex_lock(struct rt_mutex *lock)
{
	__rt_mutex_lock(lock, 0);
}
EXPORT_SYMBOL_GPL(rt_mutex_lock);

static inline void __rt_mutex_lock(struct rt_mutex *lock, unsigned int subclass)
{
	rt_mutex_lock_state(lock, subclass, TASK_UNINTERRUPTIBLE);
}

static inline int __sched rt_mutex_lock_state(struct rt_mutex *lock,
					      unsigned int subclass, int state)
{
	int ret;

	mutex_acquire(&lock->dep_map, subclass, 0, _RET_IP_);
	ret = __rt_mutex_lock_state(lock, state);
	if (ret)
		mutex_release(&lock->dep_map, 1, _RET_IP_);
	return ret;
}

int __sched __rt_mutex_lock_state(struct rt_mutex *lock, int state)
{
	might_sleep();
	return rt_mutex_fastlock(lock, state, NULL, rt_mutex_slowlock);
}
```

主流程：最终调用 rt_mutex_fastlock()，如果加锁失败则进入 rt_mutex_slowlock() 慢路径



## rt_mutex_unlock()

```c
void __sched rt_mutex_unlock(struct rt_mutex *lock)
{
	mutex_release(&lock->dep_map, 1, _RET_IP_);
	__rt_mutex_unlock(lock);
}
EXPORT_SYMBOL_GPL(rt_mutex_unlock);

void __sched __rt_mutex_unlock(struct rt_mutex *lock)
{
	rt_mutex_fastunlock(lock, rt_mutex_slowunlock);
}
```

主流程：最终调用 rt_mutex_fastunlock()，如果解锁失败则进入 rt_mutex_slowunlock() 慢路径



## rt_mutex_enqueue() 和 rt_mutex_dequeue()

```c
static void
rt_mutex_enqueue(struct rt_mutex *lock, struct rt_mutex_waiter *waiter)
{
	struct rb_node **link = &lock->waiters.rb_root.rb_node;
	struct rb_node *parent = NULL;
	struct rt_mutex_waiter *entry;
	bool leftmost = true;

	while (*link) {
		parent = *link;
		entry = rb_entry(parent, struct rt_mutex_waiter, tree_entry);
		if (rt_mutex_waiter_less(waiter, entry)) {
			link = &parent->rb_left;
		} else {
			link = &parent->rb_right;
			leftmost = false;
		}
	}

	rb_link_node(&waiter->tree_entry, parent, link);
	rb_insert_color_cached(&waiter->tree_entry, &lock->waiters, leftmost);
}

static void
rt_mutex_dequeue(struct rt_mutex *lock, struct rt_mutex_waiter *waiter)
{
	if (RB_EMPTY_NODE(&waiter->tree_entry))
		return;

	rb_erase_cached(&waiter->tree_entry, &lock->waiters);
	RB_CLEAR_NODE(&waiter->tree_entry);
}
```

将 waiter 插入/移除到 rt_mutex 的等待队列（红黑树）



## rt_mutex_adjust_prio_chain()

```c
static int rt_mutex_adjust_prio_chain(struct task_struct *task,
				      enum rtmutex_chainwalk chwalk,
				      struct rt_mutex *orig_lock,
				      struct rt_mutex *next_lock,
				      struct rt_mutex_waiter *orig_waiter,
				      struct task_struct *top_task)
{
	// ... 代码较长，已在前面详细展开 ...
	// 递归遍历优先级继承链，提升 owner 及其上游 owner 的优先级
}
```

递归调整优先级继承链，处理多级优先级反转



## rt_mutex_setprio()

```c
static void rt_mutex_setprio(struct task_struct *p, struct task_struct *pi_task)
{
	// 主要用于设置任务的优先级，通常在优先级继承时调用
	// 代码在 sched/core.c 或 sched/rt.c 中实现
}
```

设置任务的优先级，配合优先级继承



## 与调度器、task_struct 的交互

- 唤醒：rt_mutex_wake_waiter() 调用 wake_up_process() 唤醒等待者。

- 优先级提升/恢复：rt_mutex_adjust_prio()、rt_mutex_setprio() 调用调度器相关接口，提升/恢复 owner 的优先级。

- task_struct 关联：pi_waiters 记录所有等待该任务释放锁的 waiter，pi_blocked_on 指向当前任务正在等待的 waiter。