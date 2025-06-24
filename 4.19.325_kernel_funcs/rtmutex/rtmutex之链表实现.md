## rtmutex 结构体定义

在 Linux 4.19.325 内核中，rtmutex 结构体定义在 include/linux/rtmutex.h 文件中。它是 Linux 实时互斥锁（Real-Time Mutex, RT-Mutex）的核心数据结构，主要用于支持优先级继承，防止优先级反转问题。

### 结构体定义（简化版，实际代码可能有条件编译部分）

`struct rt_mutex {`

  `raw_spinlock_t     wait_lock;`

  `struct plist_head    wait_list;`

  `struct task_struct   *owner;`

`\#ifdef CONFIG_DEBUG_RT_MUTEXES`

  `void          *magic;`

`\#endif`

`\#ifdef CONFIG_RT_MUTEXES`

  `struct rt_mutex_waiter *save_waiter;`

`\#endif`

`};`

------

## 2. 各成员详细解释

### 1. raw_spinlock_t wait_lock

- 类型：raw_spinlock_t

- 作用：保护 rt_mutex 结构体内部的数据一致性。由于 RT-Mutex 可能在中断上下文中被访问，所以使用原始自旋锁（raw spinlock）来保证并发安全。

- 用途：在对 wait_list、owner 等成员进行操作时，需要先获取该锁。

------

### 2. struct plist_head wait_list

- 类型：struct plist_head

- 作用：等待该互斥锁的任务队列。plist_head 是优先级链表（priority list）的头部，链表中的元素按优先级排序。

- 用途：当有多个任务等待同一个 RT-Mutex 时，内核会将这些任务按优先级插入到 wait_list 中，优先级高的任务排在前面，便于优先唤醒。

------

### 3. struct task_struct *owner- 类型：struct task_struct*

- 作用：指向当前持有该互斥锁的任务（线程）。

- 用途：用于判断锁的归属者，以及在优先级继承时提升 owner 的优先级。

------

### 4. void *magic（仅在 CONFIG_DEBUG_RT_MUTEXES 下）- 类型：void*

- 作用：调试用的魔数（magic number），用于检测 RT-Mutex 的使用是否正确。

- 用途：在调试内核时，可以帮助发现 RT-Mutex 的非法使用或内存损坏等问题。

------

### 5. struct rt_mutex_waiter *save_waiter（仅在 CONFIG_RT_MUTEXES 下）- 类型：struct rt_mutex_waiter*

- 作用：保存当前任务的 waiter 结构体指针。

- 用途：在复杂的锁嵌套和优先级继承场景下，内核需要追踪每个任务等待的 waiter 对象，便于恢复和管理等待链。

------

## 3. 相关结构体补充

### struct plist_head

- 类型：优先级链表头部

- 作用：管理按优先级排序的等待者链表

- 用途：高优先级任务优先被唤醒

### struct rt_mutex_waiter

- 类型：等待 RT-Mutex 的任务描述结构体

- 主要成员：

- struct task_struct *task：等待的任务

- struct plist_node list_entry：用于插入到 wait_list 的节点

- struct rt_mutex *lock：等待的锁

- struct rt_mutex_waiter *pi_list_entry：用于优先级继承链

------

## 4. 作用与原理

- 优先级反转：低优先级任务持有锁，高优先级任务等待锁时，如果有中等优先级任务抢占，可能导致高优先级任务长时间得不到锁。

- 优先级继承：当高优先级任务等待锁时，锁的 owner（低优先级任务）会临时提升到高优先级，直到释放锁为止，防止优先级反转。

- PI Chain：内核通过 rt_mutex_waiter 维护一条优先级继承链（PI Chain），保证优先级传递和恢复。

  

## struct rt_mutex_waiter

### 结构体定义（位于 include/linux/rtmutex.h）

`struct rt_mutex_waiter {`

  `struct task_struct *task;      *// 等待该锁的任务*`

  `struct rt_mutex *lock;       *// 正在等待的锁*`

  `struct plist_node list_entry;    *// 用于插入到 rt_mutex 的 wait_list 的节点*`

  `struct plist_node pi_list_entry;  *// 用于插入到 task_struct 的 pi_waiters 的节点*`

  `struct rt_mutex_waiter *pi_blocked_on; *// 指向当前 waiter 正在阻塞的 waiter（PI链）*`

  `int prio;              *// 任务的优先级*`

  `int savestate;           *// 保存的状态（调试/恢复用）*`

`};`

### 主要成员说明

- task：指向正在等待锁的任务（线程）。

- lock：指向当前等待的 rt_mutex 锁。

- list_entry：用于插入到 rt_mutex 的 wait_list（优先级链表），按优先级排序。

- pi_list_entry：用于插入到 task_struct 的 pi_waiters（优先级继承链表）。

- pi_blocked_on：指向当前 waiter 正在阻塞的另一个 waiter，形成 PI Chain（优先级继承链）。

- prio：当前任务的优先级。

- savestate：保存的状态，调试或恢复用。

  

------

## struct task_struct

### 结构体定义（位于 include/linux/sched.h，内容极多，这里只列与 RT-Mutex 相关的部分）

`struct task_struct {`

  `*// ... 其他成员 ...*`

  `struct rt_mutex_waiter *pi_blocked_on; *// 当前任务被哪个 waiter 阻塞*`

  `struct plist_head pi_waiters;     *// 等待该任务释放锁的 waiter 链表（优先级链表）*`

  `struct rt_mutex *pi_lock;       *// 保护 pi_waiters 的自旋锁*`

  `int prio;               *// 动态优先级*`

  `int static_prio;            *// 静态优先级*`

  `*// ... 其他成员 ...*`

`};`

### 主要成员说明

- pi_blocked_on：指向当前任务正在等待的 rt_mutex_waiter（即该任务被哪个 waiter 阻塞）。

- pi_waiters：等待该任务释放锁的 waiter 链表（优先级链表），用于优先级继承。

- pi_lock：保护 pi_waiters 的自旋锁。

- prio/static_prio：任务的动态/静态优先级。

  

------

## 3. 三个结构体间的关联和交互

### 1. 关联关系

- rt_mutex 代表一把实时互斥锁。

- rt_mutex_waiter 代表一个等待某把锁的任务（线程）。

- task_struct 代表一个任务（线程）。

#### 具体关联

- 当一个任务（task_struct）需要获取一把 rt_mutex 锁时，如果锁已被占用，则会创建一个 rt_mutex_waiter 对象。

- 该 waiter 通过 list_entry 插入到 rt_mutex 的 wait_list（优先级链表）中。

- 同时，该 waiter 通过 pi_list_entry 插入到锁 owner 的 task_struct 的 pi_waiters 链表中。

- 如果发生优先级反转，高优先级任务的 waiter 会通过 pi_chain 传递优先级给 owner 任务。

### 2. 交互流程（优先级继承场景）

1. 任务A（高优先级）请求锁L，但锁L被任务C（低优先级）持有

- 任务A创建一个 rt_mutex_waiter，插入到锁L的 wait_list。

- 任务A的 waiter 也插入到任务C的 pi_waiters 链表。

- 任务A的 task_struct->pi_blocked_on 指向自己的 waiter。

1. 内核检查优先级继承

- 如果任务A的优先级高于任务C，任务C的优先级会被提升到任务A的优先级。

- 这种提升会沿着 PI Chain 传递，直到链的末端。

1. 任务C释放锁L

- 任务A被唤醒，成为新的 owner。

- 任务C的优先级恢复到原始值。

1. 多级嵌套

- 如果任务C也在等待另一把锁L2，且L2被任务B持有，则优先级会继续向任务B传递，形成多级 PI Chain。

  

  rt_mutex(L)         wait_list
     |-------------------|
     |                   |
  [waiter for A]   [waiter for B]
     |                   |
     |                   |
  task_struct(A)   task_struct(B)
     |                   |
  pi_blocked_on          pi_blocked_on
     |                   |
  [waiter for A]   [waiter for B]
     |                   |
  pi_list_entry          pi_list_entry
     |                   |
  task_struct(C)  (owner of L)
     |
  pi_waiters
     |
  [waiter for A] -> [waiter for B]





## 1. rt_mutex 的 wait_list

- 定义位置：struct rt_mutex 结构体内

- 类型：struct plist_head wait_list

- 内容：所有正在等待这把锁（rt_mutex）的任务的 waiter 节点

- 排序方式：按任务的优先级从高到低排序（高优先级在前）

- 作用：

- 当一个任务请求锁但锁已被占用时，会创建一个 rt_mutex_waiter，并将其插入到该锁的 wait_list 中。

- 这样可以保证高优先级任务优先获得锁。

- 本质：这是“锁视角”的等待队列，表示“谁在等我”。

------

## 2. task_struct 的 pi_waiters

- 定义位置：struct task_struct 结构体内

- 类型：struct plist_head pi_waiters

- 内容：所有等待该任务释放锁的 waiter 节点

- 排序方式：同样按优先级从高到低排序

- 作用：

- 当一个任务持有锁时，所有等待该锁的 waiter 也会被插入到 owner 任务的 pi_waiters 链表中。

- 这样可以方便地找到“哪些高优先级任务在等我释放锁”，从而实现优先级继承（owner 需要提升到最高等待者的优先级）。

- 本质：这是“任务视角”的等待队列，表示“谁因为我持锁而被阻塞”。







## rt_mutex_waiter 结构体就是 task 和 rt_mutex 的中间桥梁

### 详细解释

- rt_mutex_waiter 结构体的本质作用，就是在“等待锁的任务（task）”和“被等待的锁（rt_mutex）”之间建立联系。

- 当一个 task 需要等待一把 rt_mutex 时，内核会为这个 task 分配一个 rt_mutex_waiter 结构体。

### 这个桥梁体现在：

1. 连接 rt_mutex

- waiter 的 list_entry 字段会插入到 rt_mutex 的 wait_list 链表中，表示“我在等这把锁”。

1. 连接 task_struct

- waiter 的 task 字段指向正在等待的 task。

- task_struct 的 pi_block_on 字段会指向自己的 waiter，表示“我正通过这个 waiter 等待某把锁”。

1. 优先级继承链

- waiter 的 pi_list_entry 字段会插入到锁 owner 的 task_struct 的 pi_waiters 链表中，便于实现优先级继承。

  ## 总结

  - rt_mutex_waiter 是 task 和 rt_mutex 之间的“桥梁”或“纽带”。
  - 它让内核能够高效地管理锁的等待队列、任务的等待状态，以及优先级继承等复杂同步机制
  
  ![img](https://i-blog.csdnimg.cn/direct/78d4a62a20fb491cb98d522b7ef539ef.png)