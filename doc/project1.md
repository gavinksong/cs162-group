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

###### In thread.h

```C
struct thread {
	...
	/* Shared between thread.c, synch.c, and timer.c. */
    struct list_elem elem;		/* List element. */

	/* Owned by timer.c. */
	int64_t alarm_time;			/* Detects when a thread should wake up. */
	...
};
```

### Algorithms

When a thread calls `timer_sleep()`, it should calculate the time (in ticks since the OS booted) of when it should be woken up, and store this value in `alarm_time`. Then, it should acquire `sleepers_lock`, and use `list_insert_ordered()` to insert the current thread into the appropriate place in `sleepers`. Finally, it should call `thread_block()`.

The `timer_interrupt()` handler should disable interrupts and then check whether `sleepers_lock` is currently being held. If not, it should pop any threads off of `sleepers` that need to be unblocked and unblock them. Then, it should re-enable interrupts.

### Synchronization

We added `sleepers_lock` because if a timer interrupt occurs while a thread is traversing `sleepers`, it may pop several elements off the list and leave the thread with invalid pointers.

Since any thread that adds itself to `sleepers` blocks itself immediately afterward, it cannot exit and be deallocated. Furthermore, it should be removed from the ready list and cannot be blocked for any other reason, which is why we can safely add it to the `sleepers` list and then wake it up later.

It is possible for a thread to be interrupted after adding itself to `sleepers` but *before* blocking itself. We do not want a timer interrupt to pop an unblocked thread off of `sleepers` before the thread resumes, so we will perform a status check on every thread before removing it from the list.

### Rationale

At first, we considered implementing `sleepers` as a min-priority queue. However, we decided to use a sorted linked list because it would reduce the amount of time spent in the handler with interrupts disabled, from *O(log n)* to *O(1)*. Also, an implementation of sorted linked lists is already provided by `/lib/kernel/list.c`.


# Task 2: Priority Scheduler

### Data Structures and Functions

###### In thread.h

```C
struct thread {
	...
	/* Owned by thread.c. */
	int priority;				/* Effective priority. */
	int base_priority;			/* Base priority. */
	...
};

/* These functions will be modified to get/set the current thread's
 * base priority. This may change its effective priority, which may
 * cause it to relinquish the processor. */
void thread_set_priority (int new_priority);
void thread_get_priority (void);

/* These are the only two functions that perform inserts into the
 * ready list. They will be modified to use list_insert_ordered()
 * instead of list_push_back(). */
void thread_unblock (struct thread *t);
void thread_yield (void);
```

### Algorithms

##### 

### Synchronization
### Rationale

# Task 3: Multi-level Feedback Queue Scheduler

### Data Structures and Functions
### Algorithms
### Synchronization
### Rationale