#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/delay.h>
#include <linux/cpumask.h>
#include <linux/ktime.h>
#include <linux/completion.h>
#include <linux/rb_mutex.h>
#include <linux/kthread.h>
#include <linux/sched/types.h>


static struct rb_mutex test_lock;
DEFINE_SPINLOCK(time_measure_lock);
static struct task_struct *task_l, *task_h, *task_m;
static ktime_t h_block_start_time;
static ktime_t h_lock_acquire_time;
static struct completion l_has_lock; // 用于同步L线程获得锁
static struct completion h_is_waiting; // 确保H已开始并准备抢锁
static struct completion m_is_ready;   // 确保M已就绪，可介入翻转
static atomic_t stop_noted = ATOMIC_INIT(0);

int low_prio_thread(void *data)
{
    struct sched_param sp;
    int i;
    sp.sched_priority = 10;
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);

    pr_alert("rbmutex L: START, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    
    rb_mutex_lock(&test_lock);
    pr_alert("rbmutex L: GOT rbmutex, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    
    // 通知主线程L已经获得锁
    complete(&l_has_lock);
    // 持续运行一段时间，让M线程有机会运行
         /* 在进入长循环前，确保：
+     *  1) H 已开始并准备抢锁（阻塞在 test_lock 上）
+     *  2) M 已经就绪
+     */

    wait_for_completion_timeout(&h_is_waiting, msecs_to_jiffies(2000));
    wait_for_completion_timeout(&m_is_ready,   msecs_to_jiffies(2000));
    /*
+     * 让出一个很短的窗口（高精度 hrtimer 睡眠），
+     * 让 H 完成“阻塞在锁上”、M 开始占用 CPU，有利于稳定复现 PI。
+     */
    usleep_range(200, 400);  /* 200~400us */

    // 持续运行一段时间，让M线程有机会运行
        for (i = 0; i < 5000000; i++) {
        if (kthread_should_stop())
            break;
        if (i % 1000000 == 0) {
            pr_alert("rbmutex L: iteration=%d, current prio: %d, normal_prio: %d\n",
                    i, current->prio, current->normal_prio);
        }
        // 做一些计算工作，避免编译器优化掉循环
        __asm__ volatile("" : : : "memory");
        /*
         * L 虽然可能被 PI 提升，但也周期性给出调度点；
         * 避免在单核上因为 L 一直忙循环而推迟 H 的再次调度。
         */
        if ((i & ((1<<16)-1)) == 0)      /* ~每 65K 次迭代 */
            usleep_range(100, 200);      /* 100~200us */
        else if (need_resched())
            cond_resched();
    }
    
    rb_mutex_unlock(&test_lock);
    pr_alert("rbmutex L: RELEASE rbmutex, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    cond_resched(); /* 让出一下CPU，避免长时间占用 */

    for (i = 0; i < 5000000 && !kthread_should_stop(); i++) {
        if (i % 1000000 == 0) {
            pr_alert("rbmutex L: iteration=%d, current prio: %d, normal_prio: %d\n",
                    i, current->prio, current->normal_prio);
        }
        // 做一些计算工作，避免编译器优化掉循环
        __asm__ volatile("" : : : "memory");
                     if (need_resched())
            cond_resched();
    }
    pr_alert("rbmutex L: END, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    return 0;
}

int mid_prio_thread(void *data)
{
    struct sched_param sp;
    int i;

    sp.sched_priority = 15;
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);

    /* 告知：M 已就绪，可以介入（让 L 知道 M 已经上线） */
    complete(&m_is_ready);
    /* 短暂可中断睡眠，便于卸载时 kthread_stop() 立即生效 */
    schedule_timeout_interruptible(msecs_to_jiffies(10));

    pr_alert("rbmutex M: START, prio: %d, normal_prio: %d\n",
             current->prio, current->normal_prio);
    /*
+     * 以“时间”为节拍而不是以迭代次数为节拍，复用 usleep_range()
+     * 的高精度 hrtimer 语义，稳定地产生调度点。
+     */
    {
        ktime_t next_relax = ktime_add_us(ktime_get(), 500); /* 每 ~500us 让出一次 */
        for (i = 0; i < 1000000000 && !kthread_should_stop(); i++) {
        if (i % 100000000 == 0) {
            pr_alert("rbmutex M: iteration=%d, prio: %d, normal_prio: %d\n",
                     i, current->prio, current->normal_prio);
        }
        /* 做点无副作用的计算，制造 CPU 占用 */
            __asm__ volatile("" : : : "memory");
            /*
+             * 每 ~500us 让出 200~400us（高精度），大幅缩短 H 的阻塞时间；
+             * 若调度器有需要，也尊重 need_resched()。
+             */
            if (ktime_compare(ktime_get(), next_relax) >= 0) {
                if (kthread_should_stop())
                    break;
                usleep_range(200, 400);              /* 200~400us */
                next_relax = ktime_add_us(ktime_get(), 500);
            } else if (need_resched()) {
                cond_resched();
            }
    }
    }

    pr_alert("rbmutex M: END, prio: %d, normal_prio: %d\n",
             current->prio, current->normal_prio);
    return 0;
}

int high_prio_thread(void *data)
{
    struct sched_param sp;
    ktime_t block_duration;
    sp.sched_priority = 20;
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
    
    pr_alert("rbmutex H: START, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    
    spin_lock(&time_measure_lock);
    h_block_start_time = ktime_get();
    pr_alert("rbmutex H: Attempting to acquire lock at time: %lld ns\n", ktime_to_ns(h_block_start_time));
    spin_unlock(&time_measure_lock);
    complete(&h_is_waiting);
        if (kthread_should_stop())
       return 0;
        rb_mutex_lock(&test_lock); /* 这里可能睡眠，保持无自旋锁状态 */
    // 记录获得锁的时间
    spin_lock(&time_measure_lock);
    h_lock_acquire_time = ktime_get();
    spin_unlock(&time_measure_lock);
    pr_alert("###\n");
    pr_alert("rbmutex H: GOT rbmutex, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    pr_alert("rbmutex H: Acquired lock at time: %lld ns\n", ktime_to_ns(h_lock_acquire_time));
    pr_alert("###\n");
    // 计算阻塞时间
block_duration = ktime_sub(h_lock_acquire_time, h_block_start_time);
    pr_alert("rbmutex H: Block duration: %lld ns (%lld us)\n", 
            ktime_to_ns(block_duration), ktime_to_ns(block_duration) / 1000);
    rb_mutex_unlock(&test_lock);
    pr_alert("rbmutex H: RELEASE rbmutex, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    pr_alert("rbmutex H: END, prio: %d, normal_prio: %d\n", current->prio, current->normal_prio);
    return 0;
}

static int rbmutex_init(void)   // 初始化rb_mutex测试
{
    pr_info("rbmutex test module loaded\n");
    struct sched_param sp;
    sp.sched_priority = 25;
    sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);

    rb_mutex_init(&test_lock);
    init_completion(&l_has_lock);
 init_completion(&h_is_waiting);
    init_completion(&m_is_ready);
    // 创建低优先级线程L
    task_l = kthread_create(low_prio_thread, NULL, "low_prio_thread");
    if (IS_ERR(task_l)) {
        pr_err("Failed to create low priority thread\n");
        return PTR_ERR(task_l);
    }
    kthread_bind(task_l, 0); // 绑定到CPU 0
    wake_up_process(task_l);
    
    // 等待L线程获得锁
    wait_for_completion(&l_has_lock);
pr_info("L has lock; creating M, wait M ready, then create H\n");
    /* 先创建 M：尽快就绪并发出 m_is_ready */
    task_m = kthread_create(mid_prio_thread, NULL, "mid_prio_thread");
    if (IS_ERR(task_m)) {
        pr_err("Failed to create mid priority thread\n");
        return PTR_ERR(task_m);
    }
    kthread_bind(task_m, 0);
    wake_up_process(task_m);
    /* 给 M 1ms 窗口确保 m_is_ready 已触发 */
    wait_for_completion_timeout(&m_is_ready, msecs_to_jiffies(2000));

    /* 再创建高优先级线程 H：马上尝试加锁并阻塞在 L 的锁上 */
    task_h = kthread_create(high_prio_thread, NULL, "high_prio_thread");
    if (IS_ERR(task_h)) {
        pr_err("Failed to create high priority thread\n");
        return PTR_ERR(task_h);
    }
    kthread_bind(task_h, 0);
    wake_up_process(task_h);

    /* 主线程是 RT 25，适当让出一次 CPU，给 H/M 就绪与运行机会 */
   schedule_timeout_interruptible(msecs_to_jiffies(10));

    return 0;
}

static void rbmutex_exit(void)  // 退出rb_mutex测试
{
        /* 卸载前，先停止 H/M/L 线程，避免它们在模块文本段被释放后仍执行 */
    /* 卸载前，先停止 H/M/L 线程，避免它们在模块文本段被释放后仍执行。
     * 先唤醒，再 kthread_stop()，避免线程卡在不可中断睡眠上。
     */
    if (atomic_xchg(&stop_noted, 1) == 0)
        pr_info("rbmutex: stopping test kthreads...\n");
    if (task_h) { wake_up_process(task_h); kthread_stop(task_h); task_h = NULL; }
    if (task_m) { wake_up_process(task_m); kthread_stop(task_m); task_m = NULL; }
    if (task_l) { wake_up_process(task_l); kthread_stop(task_l); task_l = NULL; }
    pr_info("rbmutex: test kthreads stopped\n");
}


// ===================== rb_mutex 死锁检测测试 =====================
static struct rb_mutex mutexA, mutexB;
static struct task_struct *task1, *task2;

int thread1_fn(void *data)
{
    pr_alert("[DLTEST] T1: locking mutexA\n");
    rb_mutex_lock(&mutexA);
    pr_alert("[DLTEST] T1: locked mutexA, sleeping...\n");
    schedule_timeout_interruptible(msecs_to_jiffies(100));

    pr_alert("[DLTEST] T1: trying to lock mutexB with timeout\n");
    if (rb_mutex_lock_timeout(&mutexB, 100)) {
        pr_alert("[DLTEST] T1: Deadlock/timeout detected by rb_mutex!\n");
    } else {
        pr_alert("[DLTEST] T1: locked mutexB (no deadlock)\n");
        rb_mutex_unlock(&mutexB);
    }
    rb_mutex_unlock(&mutexA);
    return 0;
}

int thread2_fn(void *data)
{
    pr_alert("[DLTEST] T2: locking mutexB\n");
    rb_mutex_lock(&mutexB);
    pr_alert("[DLTEST] T2: locked mutexB, sleeping...\n");
    schedule_timeout_interruptible(msecs_to_jiffies(100));

    pr_alert("[DLTEST] T2: trying to lock mutexA with timeout\n");
   if (rb_mutex_lock_timeout(&mutexA, 100)) {
        pr_alert("[DLTEST] T2: Deadlock/timeout detected by rb_mutex!\n");
   } else {
        pr_alert("[DLTEST] T2: locked mutexA (no deadlock)\n");
        rb_mutex_unlock(&mutexA);
    }
    rb_mutex_unlock(&mutexB);
    return 0;
}

static int __init rb_mutex_deadlock_test_init(void)
{
    pr_info("[DLTEST] rb_mutex deadlock test module loaded\n");
    rb_mutex_init(&mutexA);
    rb_mutex_init(&mutexB);

    task1 = kthread_run(thread1_fn, NULL, "rbm_t1");
    task2 = kthread_run(thread2_fn, NULL, "rbm_t2");
    return 0;
}

static void __exit rb_mutex_deadlock_test_exit(void)
{
    pr_info("[DLTEST] rb_mutex deadlock test module unloaded\n");
    /* 这两个线程可能在可中断睡眠中：先唤醒再 stop */
    if (task1) { wake_up_process(task1); kthread_stop(task1); task1 = NULL; }
    if (task2) { wake_up_process(task2); kthread_stop(task2); task2 = NULL; }
    pr_info("[DLTEST] Total deadlocks detected: %d\n", rb_mutex_get_deadlock_count());
}

// 合并统一入口/出口
static int __init rb_mutex_user_init(void)
{
    int ret = 0;
    // 原有rtmutex测试
    ret = rbmutex_init();
       /* 给演示留一点时间，但不要不可中断睡眠 */
    schedule_timeout_interruptible(msecs_to_jiffies(3000));
    // 死锁测试
    rb_mutex_deadlock_test_init();
    return ret;
}

static void __exit rb_mutex_user_exit(void)
{
    /* 停止死锁测试线程再停 L/M/H，顺序不严格但建议先停可能仍在睡眠的线程 */
    rb_mutex_deadlock_test_exit();
    rbmutex_exit();
    pr_info("[DLTEST] Total deadlocks detected: %d\n", rb_mutex_get_deadlock_count());
    pr_info("rb_mutex_user module unloaded\n");
}

module_init(rb_mutex_user_init);
module_exit(rb_mutex_user_exit);
MODULE_LICENSE("GPL"); 
