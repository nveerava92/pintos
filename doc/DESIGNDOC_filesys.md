# CS 140 - Project 4: File Systems

## Design Document

## Group

**Fill in the names and email addresses of your group members.**

* FirstName LastName `<email@domain.example>`
* FirstName LastName `<email@domain.example>`
* FirstName LastName `<email@domain.example>`

## Preliminaries

Provide any preliminary comments, notes for the TAs, extra credit, or citations of consulted resources (excluding Pintos documentation, course text, lecture notes, and course staff).

---

## Indexed and Extensible Files

### Data Structures

**A1:** Copy here the declaration of each new or changed `struct`, global or static variable, `typedef`, or enumeration. Identify the purpose of each in 25 words or less.

**A2:** What is the maximum size of a file supported by your inode structure? Show your work.

### Synchronization

**A3:** Explain how your code avoids a race if two processes attempt to extend a file at the same time.

**A4:** Suppose processes A and B both have file F open, both positioned at end-of-file. If A reads and B writes F at the same time, A may read all, part, or none of what B writes—but may not read data outside what B writes. Explain how your code avoids this race.

**A5:** Explain how your synchronization design provides fairness: readers cannot indefinitely block writers, and writers cannot indefinitely block readers.

### Rationale

**A6:** Is your inode structure a multilevel index? If so, why choose this combination of direct, indirect, and doubly indirect blocks? If not, why your alternative design, and what are its advantages and disadvantages?

---

## Subdirectories

### Data Structures

**B1:** Copy here the declaration of each new or changed `struct`, global or static variable, `typedef`, or enumeration. Identify the purpose of each in 25 words or less.

### Algorithms

**B2:** Describe your code for traversing a user-specified path. How do traversals of absolute and relative paths differ?

### Synchronization

**B3:** How do you prevent races on directory entries? For example, ensure only one of two simultaneous attempts to remove the same file succeeds, or only one of two simultaneous creates of the same name succeeds.

**B4:** Does your implementation allow a directory to be removed if it is open by a process or used as its current working directory? If so, what happens to that process’s future file operations? If not, how do you prevent removal?

### Rationale

**B5:** Explain why you chose to represent a process’s current directory the way you did.

---

## Buffer Cache

### Data Structures

**C1:** Copy here the declaration of each new or changed `struct`, global or static variable, `typedef`, or enumeration. Identify the purpose of each in 25 words or less.

### Algorithms

**C2:** Describe how your cache replacement algorithm chooses a cache block to evict.

**C3:** Describe your implementation of write-behind.

**C4:** Describe your implementation of read-ahead.

### Synchronization

**C5:** When one process is reading or writing a buffer cache block, how are other processes prevented from evicting that block?

**C6:** During eviction of a block from the cache, how are other processes prevented from accessing that block?

### Rationale

**C7:** Describe a file workload likely to benefit from buffer caching, and workloads likely to benefit from read-ahead and write-behind.

---

## Survey Questions (Optional)

* Was the assignment or any part too easy or too hard? Was the workload appropriate?
* Did any particular part of the assignment enhance your OS design insight?
* Should future quarters receive specific guidance or hints?
* Suggestions for TAs to assist students more effectively?
* Additional comments?

