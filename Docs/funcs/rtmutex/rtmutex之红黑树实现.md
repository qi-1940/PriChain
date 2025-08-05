## 1. 红黑树在 rt_mutex 中的作用

- 目的：高效管理所有等待同一把 rt_mutex 的任务（waiter），并且能快速找到优先级最高的等待者。

- 优势：红黑树是一种自平衡二叉查找树，插入、删除、查找的时间复杂度都是 O(logN)，比链表在大量等待者时更高效。

------

## 2. 相关结构体

### 2.1 struct rb_root_cached

```c
struct rb_root_cached {

  struct rb_root rb_root;

  struct rb_node *rb_leftmost;

};

- rb_root：红黑树的根节点。

- rb_leftmost：指向树中最左侧的节点（即优先级最高的 waiter），便于快速唤醒。
```

### 2.2 struct rt_mutex

```c
struct rt_mutex {

  raw_spinlock_t   wait_lock;

  struct rb_root_cached waiters; *// 红黑树*

  struct task_struct *owner;

  *// ...*

};
```



### 2.3 struct rt_mutex_waiter

```c
struct rt_mutex_waiter {

  struct rb_node   tree_entry;   *// 用于插入到 rt_mutex 的 waiters 红黑树*

  struct rb_node   pi_tree_entry;  *// 用于插入到 owner task 的 pi_waiters 红黑树*

  struct task_struct *task;

  *// ...*

};
```



------

## 3. 红黑树的插入/删除/查找流程

### 3.1 插入

- 当一个 task 需要等待某个 rt_mutex 时，会创建一个 rt_mutex_waiter。

- 该 waiter 的 tree_entry 作为节点，按优先级插入到 rt_mutex 的 waiters 红黑树。

- 插入时会维护红黑树的平衡性，并更新 rb_leftmost 指针。

### 3.2 删除

- 当锁 owner 释放锁时，需要唤醒优先级最高的 waiter。

- 通过 rb_leftmost 快速找到最左节点（最高优先级 waiter），将其从红黑树中删除。

- 删除后红黑树自动调整，rb_leftmost 也会被更新。

### 3.3 查找

- 查找优先级最高的 waiter 只需访问 rb_leftmost，时间复杂度 O(1)。

- 其他查找操作（如按优先级查找某个 waiter）为 O(logN)。

![img](https://i-blog.csdnimg.cn/direct/9be7b0d9f19843da8b2c708dfa18fb4c.png)





## 红黑树结构体成员讲解

### 2.1 struct rb_root / struct rb_root_cached

定义在 include/linux/rbtree.h：

```c
struct rb_root {

  struct rb_node *rb_node;

};

struct rb_root_cached {

  struct rb_root rb_root;

  struct rb_node *rb_leftmost;

};
```

- rb_node：红黑树的根节点。

- rb_leftmost：指向树中最左侧的节点（通常是优先级最高的 waiter），便于快速查找。

### 2.2 struct rb_node

```c
struct rb_node {

  unsigned long __rb_parent_color;

  struct rb_node *rb_right;

  struct rb_node *rb_left;

};
```

- *rb_parent_color：父节点指针和颜色信息（红/黑），用位操作节省空间。*

- rb_right：右子节点指针。

- rb_left：左子节点指针。

每个 rt_mutex_waiter 结构体中都有一个 rb_node 成员（如 tree_entry 或 pi_tree_entry），用于插入到红黑树中。









# 理解红黑树的基本用法和API

include/linux/rbtree.h 和 lib/rbtree.c

# rt_mutex 如何用红黑树维护等待队列

rtmutex.c