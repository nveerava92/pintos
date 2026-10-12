# CS 140 - Project 2: User Programs

## Design Document

## Group

**Fill in the names and email addresses of your group members.**

* FirstName LastName `<email@domain.example>`
* FirstName LastName `<email@domain.example>`
* FirstName LastName `<email@domain.example>`

## Preliminaries

Provide any preliminary comments, notes for the TAs, extra credit, or citations of consulted resources (excluding Pintos documentation, course text, lecture notes, and course staff).

---

## Argument Passing

### Data Structures

**A1:** Copy here the declaration of each new or changed `struct`, global or static variable, `typedef`, or enumeration. Identify the purpose of each in 25 words or less.

### Algorithms

**A2:** Briefly describe your implementation of argument parsing. How are `argv[]` elements arranged correctly, and how is stack page overflow avoided?

### Rationale

**A3:** Why does Pintos implement `strtok_r()` but not `strtok()`?

**A4:** In Pintos, the kernel separates commands into executable name and arguments. In Unix-like systems, the shell does this separation. Identify two advantages of the Unix approach.

---

## System Calls

### Data Structures

**B1:** Copy here the declaration of each new or changed `struct`, global or static variable, `typedef`, or enumeration. Identify the purpose of each in 25 words or less.

**B2:** Describe how file descriptors are associated with open files. Are file descriptors unique across the entire OS or only within a single process?

### Algorithms

**B3:** Describe your implementation for reading and writing user data from the kernel.

**B4:** Consider a system call copying a full page (4,096 bytes) from user to kernel. What are the least and greatest possible numbers of page table inspections (`pagedir_get_page()` calls)? Repeat this for copying only 2 bytes. Can these numbers be improved?

**B5:** Briefly describe your implementation of the "wait" system call and its interaction with process termination.

**B6:** Describe your strategy to handle errors during user memory access in system calls, ensuring primary functions remain clear and resources (locks, buffers) are freed upon error. Provide an example.

### Synchronization

**B7:** How does your code ensure the "exec" system call returns -1 only after loading failure is confirmed? How is load success/failure status communicated to the calling thread?

**B8:** Consider parent process `P` and child process `C`. Explain synchronization mechanisms and how race conditions are avoided when `P` calls `wait(C)` before and after `C` exits. How do you ensure resources are freed? Address the special cases when `P` terminates without waiting before or after `C` exits.

### Rationale

**B9:** Explain your choice of kernel user-memory access implementation.

**B10:** What advantages or disadvantages exist in your file descriptor design?

**B11:** The default mapping of `tid_t` to `pid_t` is identity. If you changed it, what benefits does your approach offer?

---

## Survey Questions (Optional)

* Was the assignment or any part too easy or too hard? Was the workload appropriate?
* Did any particular aspect of the assignment enhance your OS design insight?
* Should future quarters receive specific guidance or hints?
* Suggestions for TAs to assist students more effectively?
* Additional comments?

