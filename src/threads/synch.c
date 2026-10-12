/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
*/

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static bool greater_lock (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);
static bool greater_sema (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);
static bool greater_thread (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);


/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
     decrement it.

   - up or "V": increment the value (and wake up one waiting
     thread, if any). */
void
sema_init (struct semaphore *sema, unsigned value) 
{
  ASSERT (sema != NULL);

  sema->value = value;
  list_init (&sema->waiters);
}

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. */
void
sema_down (struct semaphore *sema) 
{
  enum intr_level old_level;

  ASSERT (sema != NULL);
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  while (sema->value == 0) 
    {
      /* becase we want the WAITERS to get UNBLOCKED in decreasing order of their
         CURRENT_PRIORITY. */
      list_insert_ordered (&sema->waiters, &thread_current()->elem, &greater_thread, (NULL));
      thread_block ();
    }
  sema->value--;
  intr_set_level (old_level);
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool
sema_try_down (struct semaphore *sema) 
{
  enum intr_level old_level;
  bool success;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  if (sema->value > 0) 
    {
      sema->value--;
      success = true; 
    }
  else
    success = false;
  intr_set_level (old_level);

  return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
void
sema_up (struct semaphore *sema) 
{
  enum intr_level old_level;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  if (!list_empty (&sema->waiters)) 
  {
    /* Sort the WAITERS before UNBLOCKing a WAITER thread because
       some of them might have received PRIORITY_DONATIONs while WAITING. */
    list_sort (&sema->waiters, greater_thread, NULL);
    thread_unblock (list_entry (list_pop_front (&sema->waiters),
                                struct thread, elem));
  }
  sema->value++;

  /* We wanna yield so a higher priority thread (if any) is READY. */
  thread_yield ();
  intr_set_level (old_level);
}

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void
sema_self_test (void) 
{
  struct semaphore sema[2];
  int i;

  printf ("Testing semaphores...");
  sema_init (&sema[0], 0);
  sema_init (&sema[1], 0);
  thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
  for (i = 0; i < 10; i++) 
    {
      sema_up (&sema[0]);
      sema_down (&sema[1]);
    }
  printf ("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper (void *sema_) 
{
  struct semaphore *sema = sema_;
  int i;

  for (i = 0; i < 10; i++) 
    {
      sema_down (&sema[0]);
      sema_up (&sema[1]);
    }
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void
lock_init (struct lock *lock)
{
  ASSERT (lock != NULL);

  lock->holder = NULL;
  lock->holder_priority = 0;
  sema_init (&lock->semaphore, 1);
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
lock_acquire (struct lock *lock)
{
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (!lock_held_by_current_thread (lock));

  enum intr_level old_level = intr_disable ();
  if (lock->holder != NULL)
  {
    /* We want to donate priority to the thread (if any) that holder of LOCK
       is waiting for */
    struct thread *t = thread_current ();
    struct lock *next = lock;
    ASSERT (next->holder != t);

    /* NOTE: Deliberately manipulating CURRENT THREAD'S priority
       (both current and default) from within the LOCK object instead
       of using the thread_set_priority() procedure from the threads API.
       This is because of two reasons:
       1. thread_set_priority () preempts the THREAD_RUNNING (we still
          haven't released the lock->semaphore yet!!).
       2. The code is already too big that there's a good chance we
          might miss TIMER INTERRUPT while all of this logic executes ATOMICALLY. */
    while (next != NULL && next->holder_priority < t->current_priority)
    {
      /* We donate (if needed) our priority to the NEXT's holder */
      next->holder_priority = thread_donate_priority (next->holder);

      /* Recursively donate priority (if needed) to the any thread holding
         the lock that NEXT->holder is waiting for and so on... */
      next = next->holder->wait_lock;
    }

    /* Finally we want to indicate that we are waiting for LOCK so other threads
       wanting to acquire LOCK can donate priority (if needed) to us. */
    thread_current ()->wait_lock = lock;
  }

  sema_down (&lock->semaphore);

  intr_disable ();

  /* Now that we (the calling thread) acquired the lock, time to set the LOCK's fields to
     reflect this new reality. */
  lock->holder = thread_current ();
  lock->holder_priority = thread_current ()->current_priority;
  thread_current ()->wait_lock = NULL;
  list_insert_ordered (&thread_current ()->owned_locks_list,
                       &lock->elem, &greater_lock, NULL);

  intr_set_level (old_level);
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool
lock_try_acquire (struct lock *lock)
{
  bool success;

  ASSERT (lock != NULL);
  ASSERT (!lock_held_by_current_thread (lock));

  success = sema_try_down (&lock->semaphore);
  if (success)
    lock->holder = thread_current ();
  return success;
}

/* Releases LOCK, which must be owned by the current thread.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
void
lock_release (struct lock *lock) 
{
  ASSERT (lock != NULL);
  ASSERT (lock_held_by_current_thread (lock));
  ASSERT (thread_current ()->wait_lock == NULL);

  enum intr_level old_level = intr_disable ();

  /* We want to remove LOCK from our OWNED_LOCKS_LIST and then
     we need to give back the priority donated (if any) by
     WAITERS of the LOCK (if any). */
  list_remove (&lock->elem);
  if (list_empty (&thread_current ()->owned_locks_list))
  {
    /* Not using THREAD_RESET_PRIORITY. For reasons, read the
       next comment. */
    thread_current ()->current_priority = thread_current ()->priority;
  } else
  {
    /* Sort the elements first and get the next highest priority to adopt. */
    list_sort (&thread_current ()->owned_locks_list, greater_lock, NULL);
    int next_priority = list_entry (list_front (&thread_current ()->owned_locks_list),
                                    struct lock, elem)->holder_priority;

    /* See the big comment in LOCK_ACQUIRE () */
    /* We only want to accept the donation if it is bigger than our
       base priority. */
    if (thread_current ()->priority < next_priority)
      thread_current ()->current_priority = next_priority;
  }

  lock->holder = NULL;

  intr_set_level (old_level);
  sema_up (&lock->semaphore);
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool
lock_held_by_current_thread (const struct lock *lock) 
{
  ASSERT (lock != NULL);

  return lock->holder == thread_current ();
}

/* One semaphore in a list. */
struct semaphore_elem 
  {
    struct list_elem elem;              /* List element. */
    struct semaphore semaphore;         /* This semaphore. */
    struct thread *holder;              /* Record of the owner's priority */
  };

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void
cond_init (struct condition *cond)
{
  ASSERT (cond != NULL);

  list_init (&cond->waiters);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
cond_wait (struct condition *cond, struct lock *lock) 
{
  struct semaphore_elem waiter;

  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));
  
  sema_init (&waiter.semaphore, 0);
  waiter.holder = thread_current ();
  /* We want to UNBLOCK threads waiting for COND based on their CURRENT_PRIORITY. */
  list_insert_ordered (&cond->waiters, &waiter.elem, &greater_sema, NULL);
  lock_release (lock);
  sema_down (&waiter.semaphore);
  lock_acquire (lock);
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) 
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));

  if (!list_empty (&cond->waiters))
    sema_up (&list_entry (list_pop_front (&cond->waiters),
                          struct semaphore_elem, elem)->semaphore); 
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_broadcast (struct condition *cond, struct lock *lock) 
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);

  while (!list_empty (&cond->waiters))
    cond_signal (cond, lock);
}

static bool 
greater_sema (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED)
{
  struct semaphore_elem *sema_a, *sema_b;

  // getting the threads associated with 'a' and 'b'
  sema_a = list_entry(a, struct semaphore_elem, elem);
  sema_b = list_entry(b, struct semaphore_elem, elem);

  return sema_a->holder->current_priority > sema_b->holder->current_priority;
}

static bool
greater_lock (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED)
{
  struct lock *lock_a, *lock_b;

  // getting the threads associated with 'a' and 'b'
  lock_a = list_entry(a, struct lock, elem);
  lock_b = list_entry(b, struct lock, elem);

  // return list_entry (list_front (&lock_a->semaphore.waiters), struct thread, elem)->current_priority 
  //        > 
  //        list_entry (list_front (&lock_b->semaphore.waiters), struct thread, elem)->current_priority;

  return lock_a->holder_priority > lock_b->holder_priority;
}

static bool
greater_thread (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED)
{
  /* We need to find the threads associated with the two list elements and find out which one is less.
   * If the thread associated with LIST_ELEM 'a' has a lower priority than the thread associated with LIST_ELEM 'b',
   * then the function returns TRUE. Else, it returns FALSE. */
  struct thread *t_a, *t_b;

  // getting the threads associated with 'a' and 'b'
  t_a = list_entry (a, struct thread, elem);
  t_b = list_entry (b, struct thread, elem);

  return t_a->current_priority > t_b->current_priority;
}