# CS 140 - Project 1: Threads

## Design Document

## Group

Original coursework group:

* Naveen Veeravalli `<nveerava@buffalo.edu>`
* Saikishan Reddy Baddevolu `<saikisha@buffalo.edu>`
* Amanullah Shaik `<ashaik5@buffalo.edu>`

## Preliminaries

The original coursework implementation used the following three resources:

1. Operating Systems: Three Easy Pieces (https://pages.cs.wisc.edu/~remzi/OSTEP/), chapters 7, 8, 28, and 29. This was the primary reference in understanding how to implement a synchronization library.
2. Pintos documentation written by Stephen Tsung-Han Sher (https://www.stsher.com/teaching). This was useful for understanding the Pintos Linked List interface and enumerating cases of Priority Inversion.
3. The following video for learning how to define FIXED POINT macros: https://www.youtube.com/watch?v=4DS5E5tgxIA. This involved writing an auxiliary program outside the PA1 repository and examining its pre-processed output for the expanded macros.

This document describes the implementation in this repository, including the changes made when moving the coursework code onto the upstream Pintos base.

---

## Alarm Clock

### Data Structures

**A1:** Copy here the declaration of each new or changed `struct`, global or static variable, `typedef`, or enumeration. Identify the purpose of each in 25 words or less.

The following additional fields were added to `struct thread`:

```c
int current_priority;          /* Current priority, including donations. */
int nice;                      /* Niceness of the thread. */
int32_t recent_cpu;            /* To track latest CPU time in fixed point. */
struct list owned_locks_list;  /* Keeps a record of the locks owned by the thread. */
struct lock *wait_lock;        /* Keeps a record of the lock the thread is waiting to ACQUIRE. */
int64_t sleep_time;            /* Number of ticks the thread requested to sleep. */
int64_t start_time;            /* Time at which a thread is put to sleep. */
```

In addition to this, the following macro constants were defined for MLFQS:

```c
#define NICE_MIN -20
#define NICE_DEFAULT 0
#define NICE_MAX 20
```

A new list named `sleeping_list` is also owned by `thread.c` in addition to `all_list` and `ready_list`. This one is specifically used to keep track of all threads that put themselves to sleep.

```c
/* In thread.c */
struct list sleeping_list;

/* In thread.h */
extern struct list sleeping_list;
```

We initialize this list alongside the other lists during Pintos boot-up in `thread_init()`. There is no additional SLEEPING state. A thread that puts itself to sleep uses the existing `THREAD_BLOCKED` state.

### Algorithms

**A2:** Briefly describe what happens during a call to `timer_sleep()`, including effects from the timer interrupt handler.

Before implementing MLFQS in my coursework, I tried an approach where the thread would simply downgrade its `current_priority` and yield the CPU. The insertion into the READY queue happens in a stable fashion so two threads with the same priority are essentially scheduled in a RR fashion.

Of course, this method didn't actually block any thread wanting to sleep. If there was only one useful thread running in the OS, my implementation would essentially reduce to busy waiting. This approach fails spectacularly when we add MLFQS because threads can no longer set their own PRIORITY, which the previous model relied on.

With the final approach, `timer_sleep()` first returns immediately if TICKS is zero or negative. Otherwise we disable interrupts, record the calling thread's `sleep_time` and `start_time`, insert it into the SLEEPING LIST, and call `thread_block()`. The thread stays off the READY queue until its delay has passed.

Every time the TIMER INTERRUPT goes off, the handler checks the SLEEPING LIST and UNBLOCKs any threads whose delay has expired. After completing the wake-ups and any MLFQS updates, `thread_preempt()` checks if a higher priority thread is READY. In interrupt context, it requests a yield using `intr_yield_on_return()`; it doesn't directly call `thread_yield()`. When the sleeping thread eventually gets scheduled again, it returns from `thread_block()` and restores its previous interrupt level.

**A3:** What steps minimize the amount of time spent in the timer interrupt handler?

Because the TIMER INTERRUPT handler essentially runs while interrupts are disabled, the code it executes has to be as fast and as efficient as possible. In order to ensure that, we add the threads to the SLEEPING LIST in increasing order of their wake time using `list_insert_ordered()`.

This saves us from having to sort the SLEEPING LIST every time the interrupt handler is called. Unlike the earlier implementation that scanned the whole list, we can stop at the first thread whose sleep time hasn't elapsed:

```c
while (!list_empty (&sleeping_list))
  {
    struct thread *t = list_entry (list_front (&sleeping_list),
                                 struct thread, elem);
    if (ticks - t->start_time < t->sleep_time)
      break;
    list_pop_front (&sleeping_list);
    thread_unblock (t);
  }
```

As you can see, we remove only the threads that can be woken up and UNBLOCK them. There is no allocation or freeing of HEAP memory inside this loop.

### Synchronization

**A4:** How are race conditions avoided when multiple threads call `timer_sleep()` simultaneously?

Even if it might seem like multiple threads are calling `timer_sleep()`, a thread T has to be RUNNING in order for it to block itself. On Pintos's single CPU, only one thread runs at a time.

However, that alone isn't enough to avoid a race. A timer interrupt could still happen while the RUNNING THREAD is adding itself to the shared SLEEPING LIST. We disable interrupts across the insertion and `thread_block()` so these two operations happen atomically. Another thread cannot run in the middle of that transition.

**A5:** How are race conditions avoided when a timer interrupt occurs during a call to `timer_sleep()`?

We ensure that a timer interrupt does not happen during the process of a thread blocking itself by disabling interrupts:

```c
enum intr_level old_level = intr_disable ();
struct thread *cur = thread_current ();
cur->sleep_time = ticks;
cur->start_time = timer_ticks ();
list_insert_ordered (&sleeping_list, &cur->elem, earlier_wakeup, NULL);
thread_block ();
intr_set_level (old_level);
```

This is important because we don't want the TIMER INTERRUPT to remove and UNBLOCK a thread that hasn't actually BLOCKED itself yet. Ordered insertion involves walking the list, comparing integer wake times, and updating pointers. There is no MALLOC or FREE, although the amount of work still depends on the number of sleeping threads.

Once another thread gets scheduled, interrupts can be enabled in that thread's execution. When our sleeping thread resumes from `thread_block()`, it restores the interrupt level saved by its own call.

### Rationale

**A6:** Why did you choose this design? How is it superior to alternatives considered?

My rationale when using the earlier approach was that I didn't want to modify the SCHEDULER just to put a thread to sleep. Instead the thread downgraded its priority and yielded so other threads got scheduled. When it got scheduled once again, it checked if the timer had passed.

Of course there was a conceptual flaw with this approach, which I realized when working on MLFQS. It was still busy waiting, and a thread can be BLOCKED for a timer just as it can be BLOCKED for a synchronization construct.

With this final approach, we do the unblocking part when the external interrupt handler runs. Sleeping threads don't compete for CPU time, and the idle thread uses `sti; hlt` when there is no useful work. It doesn't busy-spin. Keeping the earliest wake time at the front also means the handler doesn't need to walk through threads that aren't ready to wake up.

---

## Priority Scheduling

### Data Structures

**B1:** Copy here the declaration of each new or changed `struct`, global or static variable, `typedef`, or enumeration. Identify the purpose of each in 25 words or less.

No changes to the structs `semaphore` and `condition`. The following fields were added to `struct lock`:

```c
struct list_elem elem;  /* To store in the owner's OWNED_LOCKS_LIST. */
int holder_priority;   /* Cached priority associated with this lock. */
```

The per-thread `current_priority`, `owned_locks_list`, and `wait_lock` fields are shown in A1. The existing `priority` field keeps the BASE priority.

The internal `struct semaphore_elem`, used for condition waiters, also gets a thread reference:

```c
struct thread *holder;  /* Thread waiting on this condition. */
```

This lets us compare CURRENT_PRIORITY even before the thread has blocked on its private semaphore.

**B2:** Explain the data structure used to track priority donation. Use ASCII art or provide a `.png` diagram.

Only one additional list was used outside of READY_LIST and the LOCK-specific WAITERS queue. This is the OWNED_LOCKS_LIST, used to keep a record of all the locks that a thread owns at a given time. Each thread owns its own version of this list that gets initialized whenever a new thread is created in `init_thread()`.

Since a thread can wait for at most one lock, we also keep a reference to that lock so other threads can donate priority if needed:

```text
H -- waiting for lock A --> M -- waiting for lock B --> L
                           owns A                     owns B

H's current_priority is donated to M, then propagated to L.
```

Whenever we get ownership of a lock, we add the lock to our list. This applies to both `lock_acquire()` and a successful `lock_try_acquire()`. The lock's semaphore WAITERS list is the source of its donations; `holder_priority` is refreshed from those live waiters rather than treated as a permanently valid snapshot.

### Algorithms

**B3:** How do you ensure the highest priority thread waiting for a lock, semaphore, or condition variable wakes first?

By inserting the WAITER into its queue in descending order of `current_priority`, using `list_insert_ordered()` just like we do with the READY queue.

This is going to be the theme for the rest of the project. Because we want to SCHEDULE according to CURRENT_PRIORITY, we keep the highest priority thread at the front. Equal priorities retain their insertion order so scheduling is essentially RR within that priority.

However, a thread's priority can change after it was inserted. We therefore sort semaphore WAITERS again in `sema_up()` and condition WAITERS again in `cond_signal()` before popping the first element. Donation also sorts READY_LIST after changing a holder's priority. Under MLFQS, we sort READY_LIST after the periodic priority updates.

After completing the state updates, `thread_preempt()` checks whether the RUNNING THREAD should give up the CPU. In ordinary thread context it yields directly; in interrupt context it requests a yield on return.

**B4:** Describe the sequence of events during priority donation in `lock_acquire()`. How is nested donation handled?

Each thread wanting to ACQUIRE a LOCK records that lock in `wait_lock`. Two scenarios are possible:

1. The lock is free, so the thread acquires it and adds it to OWNED_LOCKS_LIST.
2. The lock is held, so the thread queues itself in the semaphore WAITERS list and donates priority before BLOCKING.

The donation happens inside `sema_down()` after insertion, so the donor is already represented in the lock's WAITERS list. `thread_donate_priority()` walks through the holders:

```c
struct thread *donor = thread_current ();
for (int depth = 0; depth < 8 && donor->wait_lock != NULL; depth++)
  {
    struct thread *holder = donor->wait_lock->holder;
    if (holder == NULL)
      break;
    if (holder->current_priority < donor->current_priority)
      holder->current_priority = donor->current_priority;
    donor = holder;
  }
sort_ready_list ();
```

As you can see, we iterate because we want to donate to holders of any locks that our LOCK's HOLDER is itself WAITING for. We limit this to eight levels, as allowed by the guide. Once acquisition succeeds, we clear `wait_lock` and register ownership. All of this bookkeeping happens with interrupts disabled. MLFQS skips donation.

**B5:** Describe the sequence of events when `lock_release()` is called and a higher-priority thread is waiting.

We remove LOCK from our OWNED_LOCKS_LIST and then give back the priority donated by WAITERS of that LOCK.

In this version, `thread_refresh_priority()` starts with our BASE priority and scans the live WAITERS of every remaining owned lock. Our CURRENT_PRIORITY becomes the maximum of the BASE priority and all those remaining donations. If the list is empty, we simply revert to BASE priority.

The reason for checking live waiters is that other threads *could* have donated their priority since we acquired the lock. We don't want an old `holder_priority` value to leave us with a donation that no longer exists.

Finally we clear the released lock's holder and call `sema_up()`. This UNBLOCKs its highest priority waiter. The preemption check then allows the highest priority READY thread to run, which isn't necessarily the thread we just woke up. The ownership update, donation refresh, and semaphore release all happen with interrupts disabled.

### Synchronization

**B6:** Describe a potential race in `thread_set_priority()` and explain how your implementation avoids it. Can a lock prevent this race?

A TIMER INTERRUPT between setting BASE priority and updating CURRENT_PRIORITY could leave the scheduler observing two different realities. We disable interrupts across the change, recompute the effective priority from remaining donations, and then check for preemption.

```c
if (thread_mlfqs)
  return;
ASSERT (new_priority >= PRI_MIN && new_priority <= PRI_MAX);
enum intr_level old_level = intr_disable ();
thread_current ()->priority = new_priority;
thread_refresh_priority (thread_current ());
thread_preempt ();
intr_set_level (old_level);
```

This also handles raising BASE priority above a donation and then lowering it again. We don't just preserve whatever CURRENT_PRIORITY happened to be before the call.

Using a lock here would involve the very LOCK and priority-donation logic whose state we are trying to update. More importantly, the timer handler cannot acquire a blocking lock. Interrupt disabling is the appropriate way to protect this scheduler state.

### Rationale

**B7:** Why did you choose this design? How is it superior to alternatives considered?

It is a very simple design. OWNED_LOCKS_LIST records ownership, while the existing LOCK WAITERS lists already tell us which priorities have been donated. We don't need another separately allocated donation object for every waiter.

At the same time, we ensure that a THREAD owning multiple LOCKs "un-donates" the correct amount when it releases a lock. Rechecking live waiters costs a scan, but avoids relying on stale priority snapshots.

Strict priority scheduling isn't free of starvation. A lower priority thread can wait indefinitely while higher priority threads remain runnable. Priority donation addresses inversion around LOCKs; MLFQS addresses the separate problem of dynamically sharing CPU time.

---

## Advanced Scheduler

### Data Structures

**C1:** Copy here the declaration of each new or changed `struct`, global or static variable, `typedef`, or enumeration. Identify the purpose of each in 25 words or less.

For implementing MLFQS, I defined a new data type called `fixed_t`, which is a typedef of `int32_t` in `src/threads/fixed.h`. The file also contains function-like macros that do basic fixed-point arithmetic, although some of the macros are not used in the implementation of the BSD scheduler.

```c
typedef int32_t fixed_t;
#define FRACTION (1 << 14)
#define HALF_FRACTION (1 << 13)
```

This uses 17.14 representation. `load_avg` is a system-wide variable owned by `thread.c`:

```c
static fixed_t load_avg;
```

The per-thread `nice` and `recent_cpu` fields and NICE constants are shown in A1. The initial thread starts with nice and recent CPU of zero. Other threads inherit both from their parent, and their initial PRIORITY is calculated from those values. The idle thread keeps PRI_MIN and is excluded from CPU accounting.

### Algorithms

**C2:** Suppose threads A, B, and C have nice values 0, 1, and 2, respectively, with initial recent\_cpu values of 0. Fill in the table:

For this table, A starts RUNNING and B and C are READY in that order. Tick zero starts a fresh four-tick time slice. The values are shown after the priority recalculation and scheduling decision at each indicated tick. There are no other threads or blocking calls involved.

| Timer Ticks | recent\_cpu A | recent\_cpu B | recent\_cpu C | Priority A | Priority B | Priority C | Thread to Run |
| ----------- | ------------- | ------------- | ------------- | ---------- | ---------- | ---------- | ------------- |
| 0 | 0 | 0 | 0 | 63 | 61 | 59 | A |
| 4 | 4 | 0 | 0 | 62 | 61 | 59 | A |
| 8 | 8 | 0 | 0 | 61 | 61 | 59 | B |
| 12 | 8 | 4 | 0 | 61 | 60 | 59 | A |
| 16 | 12 | 4 | 0 | 60 | 60 | 59 | B |
| 20 | 12 | 8 | 0 | 60 | 59 | 59 | A |
| 24 | 16 | 8 | 0 | 59 | 59 | 59 | C |
| 28 | 16 | 8 | 4 | 59 | 59 | 58 | B |
| 32 | 16 | 12 | 4 | 59 | 58 | 58 | A |
| 36 | 20 | 12 | 4 | 58 | 58 | 58 | C |

**C3:** Were there ambiguities in the scheduler specification affecting the table? How were they resolved, and does this match your scheduler's behavior?

The initial READY queue order and the point at which we take the values matter when two threads have equal priority. I used the assumptions given above: A runs first, B is before C, and the values are recorded after scheduling.

The sort is stable, and the yielding thread is inserted after READY threads of equal priority. This matches the RR behavior of the implementation. No one-second recent CPU update happens within the first 36 ticks because TIMER_FREQ is 100.

**C4:** How might dividing scheduling costs between code inside and outside the interrupt context affect performance?

This is very hard to answer honestly without gathering some data. However, the TIMER INTERRUPT work has to stay short because it takes CPU time away from the thread being interrupted.

Every tick, we increment recent CPU for the RUNNING THREAD unless it is the idle thread. At every exact multiple of TIMER_FREQ, we update load average and then recent CPU for all non-idle threads. Independently, at every fourth tick, we update their priorities and sort READY_LIST.

All these updates happen before an ordinary thread resumes, so it cannot see a new timer tick with old scheduler values. The actual context switch happens on interrupt return when requested. We don't sort the ready list again on every preemption check; it is already maintained in priority order. That keeps unnecessary work out of the timer handler.

### Rationale

**C5:** Critique your design: identify advantages and disadvantages. How would you refine or improve it given more time?

The biggest advantage is that we use the same READY_LIST for both scheduling policies. A single ordered list is easier to reason about than maintaining 64 queues, and the guide allows it as long as the behavior is the same.

The disadvantage is that ordered insertion takes time proportional to the number of READY threads, and periodic sorting also adds work while interrupts are disabled. If I had more time, I would compare this against 64 FIFO queues using actual measurements before changing the design.

MLFQS is independent of priority donation. Calls to `thread_set_priority()` and the priority argument supplied to `thread_create()` don't determine MLFQS priority. Nice and inherited recent CPU do. We truncate the recent CPU term when calculating priority, clamp the result to PRI_MIN through PRI_MAX, and only round to nearest for the public recent CPU and load-average values multiplied by 100.

**C6:** Why did you implement fixed-point math as you did? Did you create an abstraction layer (data types/functions/macros)? Why or why not?

My reasoning for using macros instead of functions was to avoid the overhead of a function call. Macros simply expand in place. I learnt how to define the FIXED POINT macros by doing trial and error verification, including examining the pre-processed file using `gcc -E` to ensure the expanded expressions worked as intended.

The macros give us one place to handle conversions, scaling, and rounding. Multiplication and division between two fixed-point values use a 64-bit intermediate so the calculation doesn't overflow before we scale the result back down. The complete expressions and their arguments are enclosed in parentheses so macro expansion doesn't change the intended operator precedence.

Of course, macros can evaluate their arguments multiple times, so I don't pass expressions with side effects. Inline functions could also avoid a call while evaluating each argument once. The implementation keeps the macro approach from my coursework; avoiding function calls alone isn't a guarantee against stack overflow.

---

## Survey Questions (Optional)

**Was the assignment or any part too easy or too hard?**

I think MLFQS was the most-straigtforward to understand and implement but very
tricky to test or even be able to diagnose the problems based on the error MSGs
or lack thereof.

Priority scheduling was challenging but the actual time I spent coding it
was very small in comparision to the time spent enumerating all the possible
control flows, deciding how to clamp down these unpredictable control flows
through known function calls like thread_yield (), thread_unblock ().

**Did any particular aspect of the assignment enhance your OS design insight?**

Yes, most certainly. The part on Priority Inversion was a great mental exercise.
I felt like I would have implemented it more quickly if I had access to a giant
whiteboard big enough to fit multiple flow diagrams and such.

**Should future quarters receive specific guidance or hints?**

I think the material taught by the professor in class is a highly generalized view of looking at the problems of scheduling and synchronization. In reality, a lot of those generalized control flows simply don't exist because of our design choices. For example, the main program initializes the idle thread using a semaphore, which solves the chicken and egg problem. Another example is locks building on top of semaphores. In Project I, we had to make the waiter selection and scheduler respect priorities instead of assuming the existing synchronization code already did so.
