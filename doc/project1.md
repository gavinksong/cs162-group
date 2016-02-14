Design Document for Project 1: Threads
======================================

## Group Members

* Keonhwa Song <gavinsong93@berkeley.edu>
* Jun Tan <email@domain.example>
* Pengcheng Liu <shawnpliu@berkeley.edu>
* Jacobo Sternberg <j.sternberg@berkeley.edu>

# Task 1: Efficient Alarm Clock

	## Data Structures and Functions

		### In timer.c

		```
		static struct list sleepers;
		```
		List of all sleeping threads, sorted by `alarm_time`.

		```
		static struct lock sleepers_lock;
		```
		Lock on sleepers list used by `timer_sleep()` and `timer_interrupt()`.

		### In thread.h

		```
		struct thread {
			...

			/* Owned by timer.c. */
			int64_t alarm_time;
			
			...
		}
		```
		The thread should be woken up when `timer_ticks()` surpasses this value.

	## Algorithms

		When a thread calls `timer_sleep()`, it should calculate the time (in ticks since the OS booted) of when it should be woken up, and store this value in `alarm_time`. Then, it should acquire `sleepers_lock`, and use `list_insert_ordered()` to insert the current thread into the appropriate place in `sleepers`. Finally, it should call `thread_block()`.

		The `timer_interrupt()` handler should disable interrupts and then check whether `sleepers_lock` is currently being held. If not, it should pop any threads off of `sleepers` that need to be unblocked and unblock them. Then, it should re-enable interrupts.

	## Synchronization

		We added `sleepers_lock` because if a timer interrupt occurs while a thread is traversing `sleepers`, it may pop several elements off the list and leave the thread with invalid pointers.

		Meanwhile, since any thread that adds itself to `sleepers` blocks itself immediately afterward, it cannot exit and be deallocated. Furthermore, it should be removed from the ready list and cannot be blocked for any other reason, which is why we can safely add it to the `sleepers` list and then wake it up later.

	## Rationale

		At first, we considered implementing `sleepers` as a min-priority queue. However, we decided to use a sorted linked list because it would reduce the amount of time spent in the handler with interrupts disabled, from *O(log n)* to *O(1)*. Also, an implementation of sorted linked lists is already provided by `/lib/kernel/list.c`.


# Task 2: Priority Scheduler

	## Data Structures and Functions
	## Algorithms
	## Synchronization
	## Rationale

# Task 3: Multi-level Feedback Queue Scheduler

	## Data Structures and Functions
	## Algorithms
	## Synchronization
	## Rationale