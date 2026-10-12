# CS 140 - Project 3: Virtual Memory

## Design Document

## Group

**Fill in the names and email addresses of your group members.**

* FirstName LastName `<email@domain.example>`
* FirstName LastName `<email@domain.example>`
* FirstName LastName `<email@domain.example>`

## Preliminaries

Provide any preliminary comments, notes for the TAs, extra credit, or citations of consulted resources (excluding Pintos documentation, course text, lecture notes, and course staff).

---

## Page Table Management

### Data Structures

**A1:** Copy here the declaration of each new or changed `struct`, global or static variable, `typedef`, or enumeration. Identify the purpose of each in 25 words or less.

### Algorithms

**A2:** In a few paragraphs, describe your code for locating the frame, if any, that contains the data of a given page.

**A3:** How does your code coordinate accessed and dirty bits between kernel and user virtual addresses that alias a single frame, or alternatively how do you avoid the issue?

### Synchronization

**A4:** When two user processes both need a new frame at the same time, how are races avoided?

### Rationale

**A5:** Why did you choose the data structure(s) for representing virtual-to-physical mappings?

---

## Paging to and from Disk

### Data Structures

**B1:** Copy here the declaration of each new or changed `struct`, global or static variable, `typedef`, or enumeration. Identify the purpose of each in 25 words or less.

### Algorithms

**B2:** When a frame is required but none is free, describe your code for choosing a frame to evict.

**B3:** When a process `P` obtains a frame previously used by process `Q`, how do you adjust the page table (and any other data structures) to reflect that `Q` no longer has it?

**B4:** Explain your heuristic for deciding whether a page fault for an invalid virtual address should cause the stack to be extended into the faulted page.

### Synchronization

**B5:** Explain the basics of your VM synchronization design, particularly how it prevents deadlock.

**B6:** A page fault in process `P` can cause another process `Q`'s frame to be evicted. How do you ensure that `Q` cannot access or modify the page during eviction? How do you avoid a race between `P` evicting `Q`'s frame and `Q` faulting the page back in?

**B7:** Suppose a page fault in process `P` causes a page to be read from the file system or swap. How do you ensure that a second process `Q` cannot interfere by attempting to evict the frame while it is still being read?

**B8:** Explain how you handle access to paged-out pages during system calls. Do you use page faults to bring pages in, lock frames into memory, or another design? How do you gracefully handle attempted accesses to invalid virtual addresses?

### Rationale

**B9:** A single lock for the whole VM system simplifies synchronization but limits parallelism, whereas many locks complicate synchronization and risk deadlock but allow high parallelism. Explain where your design falls and why.

---

## Memory Mapped Files

### Data Structures

**C1:** Copy here the declaration of each new or changed `struct`, global or static variable, `typedef`, or enumeration. Identify the purpose of each in 25 words or less.

### Algorithms

**C2:** Describe how memory mapped files integrate into your virtual memory subsystem. Explain how page fault and eviction processes differ between swap pages and other pages.

**C3:** Explain how you determine whether a new file mapping overlaps any existing segment.

### Rationale

**C4:** Mappings created with "mmap" have semantics similar to demand-paged executables but write back to original files rather than swap. Explain why your implementation does or does not share code for these situations.

---

## Survey Questions (Optional)

* Was the assignment or any part too easy or too hard? Was the workload appropriate?
* Did any particular part of the assignment enhance your OS design insight?
* Should future quarters receive specific guidance or hints?
* Suggestions for TAs to assist students more effectively?
* Additional comments?

