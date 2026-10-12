# CS 140 - Project 1: Threads

## Design Document

## Group

**Fill in the names and email addresses of your group members.**

* FirstName LastName `<email@domain.example>`
* FirstName LastName `<email@domain.example>`
* FirstName LastName `<email@domain.example>`

## Preliminaries

Provide any preliminary comments, notes for the TAs, extra credit, or citations of consulted resources (excluding Pintos documentation, course text, lecture notes, and course staff).

---

## Alarm Clock

### Data Structures

**A1:** Copy here the declaration of each new or changed `struct`, global or static variable, `typedef`, or enumeration. Identify the purpose of each in 25 words or less.

### Algorithms

**A2:** Briefly describe what happens during a call to `timer_sleep()`, including effects from the timer interrupt handler.

**A3:** What steps minimize the amount of time spent in the timer interrupt handler?

### Synchronization

**A4:** How are race conditions avoided when multiple threads call `timer_sleep()` simultaneously?

**A5:** How are race conditions avoided when a timer interrupt occurs during a call to `timer_sleep()`?

### Rationale

**A6:** Why did you choose this design? How is it superior to alternatives considered?

---

## Priority Scheduling

### Data Structures

**B1:** Copy here the declaration of each new or changed `struct`, global or static variable, `typedef`, or enumeration. Identify the purpose of each in 25 words or less.

**B2:** Explain the data structure used to track priority donation. Use ASCII art or provide a `.png` diagram.

### Algorithms

**B3:** How do you ensure the highest priority thread waiting for a lock, semaphore, or condition variable wakes first?

**B4:** Describe the sequence of events during priority donation in `lock_acquire()`. How is nested donation handled?

**B5:** Describe the sequence of events when `lock_release()` is called and a higher-priority thread is waiting.

### Synchronization

**B6:** Describe a potential race in `thread_set_priority()` and explain how your implementation avoids it. Can a lock prevent this race?

### Rationale

**B7:** Why did you choose this design? How is it superior to alternatives considered?

---

## Advanced Scheduler

### Data Structures

**C1:** Copy here the declaration of each new or changed `struct`, global or static variable, `typedef`, or enumeration. Identify the purpose of each in 25 words or less.

### Algorithms

**C2:** Suppose threads A, B, and C have nice values 0, 1, and 2, respectively, with initial recent\_cpu values of 0. Fill in the table:

| Timer Ticks | recent\_cpu A | recent\_cpu B | recent\_cpu C | Priority A | Priority B | Priority C | Thread to Run |
| ----------- | ------------- | ------------- | ------------- | ---------- | ---------- | ---------- | ------------- |
| 0           |               |               |               |            |            |            |               |
| 4           |               |               |               |            |            |            |               |
| 8           |               |               |               |            |            |            |               |
| 12          |               |               |               |            |            |            |               |
| 16          |               |               |               |            |            |            |               |
| 20          |               |               |               |            |            |            |               |
| 24          |               |               |               |            |            |            |               |
| 28          |               |               |               |            |            |            |               |
| 32          |               |               |               |            |            |            |               |
| 36          |               |               |               |            |            |            |               |

**C3:** Were there ambiguities in the scheduler specification affecting the table? How were they resolved, and does this match your scheduler's behavior?

**C4:** How might dividing scheduling costs between code inside and outside the interrupt context affect performance?

### Rationale

**C5:** Critique your design: identify advantages and disadvantages. How would you refine or improve it given more time?

**C6:** Why did you implement fixed-point math as you did? Did you create an abstraction layer (data types/functions/macros)? Why or why not?

---

## Survey Questions (Optional)

* Was the assignment or any part too easy or too hard? Was the workload appropriate?
* Did any particular aspect of the assignment enhance your OS design insight?
* Should future quarters receive specific guidance or hints?
* Suggestions for TAs to assist students more effectively?
* Additional comments?

