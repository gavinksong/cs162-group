Design Document for Project 1: Threads
======================================

## Group Members

* Keonhwa Song <gavinsong93@berkeley.edu>
* Jun Tan <jtan0325@berkeley.edu>
* Pengcheng Liu <shawnpliu@berkeley.edu>
* Jacobo Sternberg <j.sternberg@berkeley.edu>

# Task 1: Efficient Alarm Clock

### Data Structures and Functions

###### In timer.c

```C
/* List of all sleeping threads, sorted by `alarm_time`. */
static struct list sleepers;

/* Lock on sleepers list used by `timer_sleep()` and `timer_interrupt()`. */
static struct lock sleepers_lock;
```

###### In thread.c

```C
struct thread {
	...
	/* Shared between thread.c, synch.c, and timer.c. */
	struct list_elem elem;			/* List element. */

	/* Owned by timer.c. */
	int64_t alarm_time;				/* Detects when a thread should wake up. */
	...
};

/* This function needs to be modified to initialize the new struct fields */
static void init_thread (struct thread *t, const char *name, int priority);
```

### Algorithms

When a thread calls `timer_sleep()`, it should calculate the time (in ticks since the OS booted) of when it should be woken up, and store this value in `alarm_time`. Then, it should acquire `sleepers_lock`, and use `list_insert_ordered()` to insert the current thread into the appropriate place in `sleepers`. Finally, it should call `thread_block()`.

The `timer_interrupt()` handler should check whether `sleepers_lock` is currently being held. If not, it should pop any threads off of `sleepers` that need to be unblocked and unblock them.

### Synchronization

We added `sleepers_lock` because if a timer interrupt occurs while a thread is traversing `sleepers`, it may pop several elements off the list and leave the thread with invalid pointers. On the other hand, `timer_interrupt()` itself runs in an external interrupt context, so while it will check whether the lock is held, it does not need to acquire the lock.

Since any thread that adds itself to `sleepers` blocks itself immediately afterward, it cannot exit and be deallocated. Furthermore, it should be removed from the ready list and cannot be blocked for any other reason, which is why we can safely add it to the `sleepers` list and then wake it up later.

However, it is possible for a thread to be interrupted after adding itself to `sleepers` but *before* blocking itself. We do not want a timer interrupt to pop an unblocked thread off of `sleepers`, so we will perform a status check on every thread before removing it from the list.

### Rationale

At first, we considered implementing `sleepers` as a min-priority queue. However, we decided to use a sorted linked list because it would reduce the amount of time spent in the handler with interrupts disabled, from *O(log n)* to *O(1)*. Also, an implementation of sorted linked lists is already provided by `/lib/kernel/list.c`.


# Task 2: Priority Scheduler

### Data Structures and Functions

###### In thread.c

```C
struct thread {
	...
	/* Owned by thread.c. */
	fixed_point_t priority;			/* Effective priority. */
	fixed_point_t base_priority;	/* Base priority. */
	
	/* Owned by synch.c. */
	struct list held_locks;			/* List of locks held by this thread. */
	...
};

/* This function needs to be modified to initialize the new struct fields */
static void init_thread (struct thread *t, const char *name, int priority);

/* This function will be modified to select the thread with the max priority */
static struct thread *next_thread_to_run (void);

/* These functions will be modified to get or set the current thread's
 * base priority. */
void thread_set_priority (int new_priority);
int thread_get_priority (void);
```

###### In synch.c

```C
struct lock {
	...
	fixed_point_t priority;			/* The max priority of waiters. */
	struct list_elem elem;			/* List element for held locks list. */
};

/* These functions will be modified. */
void sema_up (struct semaphore *);
void lock_init (struct lock *);
void lock_acquire (struct lock *);
bool lock_try_acquire (struct lock *);
void lock_release (struct lock *);
```

### Algorithms

#### Choosing the next thread to run
We will modify `next_thread_to_run()` to use `list_max()` instead of `list_pop_front()` to find the next thread to run. After being chosen, the thread will be popped off of the ready list with `list_remove()`.

#### Choosing the next thread to unblock
Since monitors are implemented with locks, and locks are implemented with semaphores, we only need to implement priority scheduling inside `sema_up()`. We will use `list_max()` to grab the waiter with the highest priority and unblock it.

#### Priority donations
Our implementation of priority donations will be two-fold. First, we will maintain a "priority" value for each lock which represents the highest priority of its waiters. Then, the effective priority of each thread will simply be the highest priority of its held locks (or the thread's base priority if it is higher). We inserted this layer of indirection in order to reduce the amount of uninterrupted computation required for `lock_release()`.

##### When a thread begins waiting for a lock
There are only two instances where the priority of a lock will increase:

- When a thread begins waiting for a lock, and the priority of the lock is less than the priority of the thread.
- If a thread’s priority has been increased, and it is waiting on a lock whose priority is less than the thread’s updated priority.

If either of these events increase a lock's priority, then its holder’s priority is also increased if it is less than the priority of the lock. Conveniently, this happens to be the only way the second event can occur; a blocked thread's priority cannot change by any other means. Thus, all three of these updates may be implemented within `lock_acquire()`.

##### When a lock is released
A lock's priority can only decrease when it is released. In `lock_release()`, after the lock calls `sema_up()`, we will use `list_max()` to get the highest priority of the waiters and update the lock's priority to that value. The priority of the thread that released the lock must also be reset to the maximum priority of any remaining locks it may be holding (or its base priority). Since a thread's priority may decrease in this way, we will defensively call `thread_yield()`.

We can obviously avoid computing an actual max most of the time, but I won't detail the step-by-steps here.

##### When a lock is acquired

Notice that we do not have to do anything when a thread acquires a lock, other than to add the lock to the holder's `held_locks` list. Since a semaphore will unblock the waiter with the highest priority when a lock is released, the next thread to acquire the lock is guaranteed to have a priority greater than or equal to this priority.

#### Resetting the base priority
The base priority of the current thread can be set through `thread_set_priority()`. The thread's new effective priority becomes the max of the updated base priority and the priority of any held locks. If this value is lower than the old priority value, the thread defensively calls `thread_yield()`.

Similarly to when we release a lock, we can avoid computing a max most of the time.

### Synchronization

The priority scheduling segments of our design are already safe from interrupts since they run inside `next_thread_to_run()` and `sema_up()`.

On the other hand, any code that deals with priority donations must be made atomic. We tried compiling a list of all the places where an interrupt might cause our data to become inconsistent, but it became too long and complicated. Furthermore, we obviously cannot use locks to protect the priority values of locks. That is why we decided to simply disable interrupts whenever priority values are being updated, including `thread_set_priority()`.

### Rationale

##### Ready list
We considered three implementations: using a priority queue, or using a linked list with either `list_insert_ordered()` or `list_max()`. We chose the last option because, despite being slow, it has two huge advantages. First, the underlying data structure does not need to be sorted (or re-sorted every time effective priority values change). Second, we only need to change the code inside `next_thread_to_run()`, which is already required to have interrupts disabled on entry. This makes max removal simpler and safer to implement than the other two solutions.

##### Waiters list
We considered similar options for the waiters list in a semaphore. Between using sorted inserts and computing maximums, we again decided on the latter for its simplicity. With sorted inserts, we would have to perform additional re-inserts each time a waiter's priority value changed. This may occur multiple times with each `lock_acquire()` if there are many waiters, whereas if we computed maximums, we would only have to do so twice with each `lock_release()`.

##### Lock priority
We decided to implement this to avoid performing an extra level of recursion in `lock_release()`. When a thread releases a lock, it may lose the effects of a priority donation. If this occurs, it needs to find the value of the next effective priority donation. In other words, it needs to find the maximum priority of all threads that are waiting for any of the locks being held by the current thread. With lock priorities, it can do this just by maxing over the priorities of the locks, rather than having to also max over the priorities of the waiters of each lock.

In a way, the reason we are keeping track of lock priorities is the same as the reason we are keeping track of the effective priorities of threads instead of dynamically recomputing them.

# Task 3: Multi-level Feedback Queue Scheduler

### Data Structures and Functions

###### In thread.c

```C
#define NUM_QUEUES 64

/* MLFQS queues. */
static struct list ready_queues[NUM_QUEUES];

/* Index of the highest priority non-empty MLFQS queue. */
static int queue_index;

/* Load average. */
static fixed_point_t load_avg;

struct thread {
	...
	/* For MLFQS. */
	fixed_point_t nice;				/* Niceness value. */
	fixed_point_t recent_cpu;		/* Recent CPU value. */
};

/* This function needs to be modified to initialize load_avg and ready_queues. */
void thread_init (void);

/* This function needs to be modified to initialize the new struct fields. */
static void init_thread (struct thread *t, const char *name, int priority);

/* This function will be modified for MLFQS. */
static struct thread *next_thread_to_run (void);

/* We will update all MLFQS values in here. */
void thread_tick (void);
```

### Algorithms

##### At every thread tick
We will increment the current thread's `recent_cpu` value. If `ticks` is divisible by 4, we will recompute the thread's priority and clamp it between `PRI_MIN` and `PRI_MAX`. If `ticks` is divisible by `TIMER_FREQ`, we will update `load_avg`, recompute the priorities of all threads, and redistribute the threads to the correct queues. We can perform these last two steps in one pass over `all_list`. For each thread, we will recompute its priority, and then send it to the back of the appropriate ready queue. As we do this, we will also determine the highest new priority value and store it in `queue_index`. If there are no non-idle threads, we will set `queue_index` to -1.

##### Choosing the next thread to run
First, the current thread should be pushed to the back of the appropriate ready queue. If the index of this queue is higher than `queue_index`, we will update `queue_index` to this value. Next, we will check the queue at index `queue_index`. If it is non-empty, we will pop the next thread in this queue and return it. Otherwise, we will decrement `queue_index`, and try again. If `queue_index` reaches -1, we will return the idle thread.

### Synchronization
All the code runs inside the timer interrupt, so there shouldn't be any synchronization issues.

### Rationale

Our strategy was to try to put most of the computation in the per-second update. We quickly realized that the priority of ready threads only had to be updated once per second, since `nice` and `recent_cpu` values can only change for the running thread. This also meant we only had to move READY threads between queues once per second. And this in turn implied that the index of the highest priority non-empty queue only changed if the priority of the RUNNING thread had changed. We exploited this by confining the code for updating and moving READY threads to the per-second update and keeping track of a `queue_index`.

##### Queue index
We chose to keep track of `queue_index` rather than try to find the non-empty queue with the highest priority with each call to `next_thread_to_run()`. Between per-second updates, keeping track of this index is easy because the priority of threads only change while they are running on the processor. Since thread-switches happen 25 times per second, the total amount of time saved during thread-switching is definitely worth having to recompute `queue_index` every second.

##### Iterating over all list
When we recompute all priorities and redistribute across the ready queues, we could have chosen to make a pass over each ready queue in descending order of priority. Instead, we chose to iterate over `all_list`. Although the first option has the advantage of preserving the order of threads in the queues, we chose the latter option for its simplicity. Admittedly, this may cause threads positioned closer to the head of `all_list` to receive slightly more preference. However, we are hoping that the mechanics of MLFQS will alleviate this effect.

# Additional Questions
1.	**Test setup**: Initialize a lock and a semaphore at value 0. Start a thread T0 (priority 31) to launch two threads, T1 (priority 41) and T2 (priority 51), and then call `thread_yield()`. T2 will immediately down the semaphore. T1 will acquire the lock, launch thread T3 (priority 61), and then down the semaphore. T3 will call `lock_acquire()` on the lock. T0 will then call `sema_up()`, `thread_yield()`, and then `sema_up()` again. T1 and T2 will respectively print "T1" and "T2" before exiting.

	**Explanation**: Since thread T0 calls `thread_yield()` right after launching T1 and T2, it will only resume after T1 and T2 both either block or exit. T2 blocks immediately because it downs the zero semaphore. T1 launches T3 after first acquiring the lock, so T3 must wait for the lock and should donate its priority to T1. Only after T2 and T3 both block, T0 resumes and calls `sema_up()` and then yields. This should cause either T1 or T2 to unblock and print. Then, T0 will call `sema_up()` again to release the remaining thread(s).

	**Expected output**: "T1" should be printed before "T2". Since T1 receives a priority donation from T3, the semaphore should choose T1 over T2 when `sema_up()` is called for the first time.

	**Actual output**: "T2" will be printed before "T1". Since the actual implementation of `sema_up()` is based on base priority, it will pick T2 over T1 when `sema_up()` is first called.

2. 
timer ticks | R(A) | R(B) | R(C) | P(A) | P(B) | P(C) | thread to run
------------|------|------|------|------|------|------|--------------
 0          |  0   |  0   |   0  |   63 |   61 |  59  | P(A)
 4          |  1   |  0   |   0  |   62 |   61 |  59  | P(A)
 8          |  2   |  0   |   0  |   61 |   61 |  59  | P(B)
12          |  2   |  1   |   0  |   61 |   60 |  59  | P(A)
16          |  3   |  1   |   0  |   60 |   60 |  59  | P(B)
20          |  3   |  2   |   0  |   60 |   59 |  59  | P(A)
24          |  4   |  2   |   0  |   59 |   59 |  59  | P(C)
28          |  4   |  2   |   1  |   59 |   59 |  58  | P(B)
32          |  4   |  3   |   1  |   59 |   58 |  58  | P(A)
36          |  5   |  3   |   1  |   58 |   58 |  58  | P(C)

3. 
