# CS 140 - Project 2: User Programs

## Design Document

## Group

Original coursework author:

* Naveen Veeravalli `<nveerava@buffalo.edu>`

## Preliminaries

Phase 3 requires passing 80 tests. The work submitted to Autolab passed 54 out of 80. This repository ports that coursework implementation while keeping our current Threads implementation. The port reproduces the same 54 passes and 26 failures, and all 27 Threads tests continue to pass.

This document describes that port as it exists now, including the incomplete parts. It doesn't describe a finished Project 2 solution.

The resources I consulted for the original coursework were:

1. [GDB Documentation](https://ftp.gnu.org/old-gnu/Manuals/gdb/html_node/gdb_toc.html), used extensively for Phase 1 and for the generic read and write system calls in Phase 3.
2. [Stack-pointer alignment using bitwise operators](https://stackoverflow.com/questions/62986886/assuring-stack-pointer-alignment-using-bitwise-operators), for aligning ESP to the x86 word boundary.

---

## Argument Passing

### Data Structures

**A1:** Copy here the declaration of each new or changed `struct`, global or static variable, `typedef`, or enumeration. Identify the purpose of each in 25 words or less.

The provided `struct thread` was extended for this part so it can support `USER` processes. Please check the code snippet and the associated comments for my reasons to include them:
```c
struct thread
{

// Fields owned by threads/thread.c
// ...

#ifdef USERPROG

/* Owned by userprog/process.c. */
/* Page directory. */
uint32_t *pagedir; 
// Reference to parent thread
struct thread* parent; 
// Notification used for loading and exit coordination
struct semaphore sema_child; 
// Reference to the executable running in the thread.
struct file* exec;
// List of files opened by EXEC during execution.
struct list files;
// List of CHILD threads spawned by the thread.
struct list children;
// To include in the list PARENT->children.
struct list_elem child_elem;
// To keep track of the user process's execution status.
int32_t return_status;
// Whether the user_process is done executing or not.
bool exited;
// Whether the parent of the user_process is waiting or not.
bool parent_waiting;
// Coordination between process_wait and process_exit.
struct semaphore sema_exit; 

#endif

// ...
};
```

The process fields are initialized in `thread_create()` under `#ifdef USERPROG`. The initial kernel thread doesn't go through this same setup, and child-list insertion skips that initial thread. This is the coursework's initialization scheme; it isn't yet a complete process-lifetime design.

The return values are represented by the following macros in `thread.h`:

```c
#define RETURN_INIT 0
#define RETURN_SUCCESS 0
#define RETURN_ERROR -1
```

Of course, RETURN_INIT and RETURN_SUCCESS having the same value means that the status by itself cannot tell us whether execution is still in progress. The implementation also checks `exited`.

### Algorithms

**A2:** Briefly describe your implementation of argument parsing. How are `argv[]` elements arranged correctly, and how is stack page overflow avoided?

Argument passing is implemented in the function `set_program_stack ()` defined in `userprog/process.c`. It is called by the *thread* in which the USER program will run. This is not the PARENT itself, rather it is the thread spawned when the PARENT calls `process_execute ()`.

The function is only called after `load ()` succeeds. Before that, `process_execute ()` copies the command line into a kernel page so the child isn't parsing a buffer that the caller could change. `start_process ()` extracts the first token using `strtok_r ()` and passes the remaining parsing state to `set_program_stack ()`.

Currently, the arguments are stored as strings in the *kernel* memory. Which means, we have to copy them into the USER process's virtual memory before setting up `argv[]` and `argc`:

```c
while (token != NULL)
  {
    size_t len = strlen (token) + 1;
    stack_pointer = (void *) (((char *) stack_pointer) - len);
    strlcpy ((char *) stack_pointer, token, len);
    if (PHYS_BASE - stack_pointer > MAX_ARGS_SIZE)
      return 0;
    argc++;
    token = strtok_r (NULL, " ", save_ptr);
  }
```

I was already aware of `strtok_r ()` and its non-reentrant version `strtok ()` because I used the latter extensively to parse messages exchanged between a client and server application I built for Computer Networks. I know *why* tokenization destroys the original string: it replaces delimiters with NULL characters and returns pointers into that same buffer. Multiple spaces are skipped, so they don't create empty arguments.

Every time we copy something onto the stack, the local STACK_POINTER moves down across the USER VM address space. MAX_ARGS_SIZE is defined as 4096 to represent 4KB. However, the check shown above happens **after** the string is copied, and it only accounts for the strings. The pointer array, alignment, and the final words can still go beyond the page. So this version attempts to prevent overflow, but doesn't safely guarantee it.

Now, before setting up the arguments themselves, we align the pointer to a word boundary:

```c
stack_pointer = (void *) (((intptr_t) stack_pointer) & 0xfffffffc);
```

For a 4B word size, the address of a word's first byte is a multiple of 4. In binary, it ends in `00`. Since we are moving down the address space, clearing the two LSBs rounds the pointer down to the next word boundary. I remember solving similar problems in Computer Architecture when studying MIPS. I also checked the Stack Overflow reference listed in the Preliminaries and verified the stack using `pintos-gdb`.

The stack page is allocated using `PAL_USER | PAL_ZERO`, so it starts filled with zeros. We reserve a word for the NULL sentinel but explicitly write only one byte of it; the other bytes remain zero from that initialization. After that, we walk through the stored strings from the lowest address and push their pointers. Because each pointer push moves down the stack, this puts `argv[0]` at the lowest address of the final pointer array.

Finally, we push the address of that array, the integer argc, and a fake return address to satisfy the x86 calling convention. `*esp` is updated to this final STACK_POINTER. All five supplied argument-passing tests pass, although larger inputs still need proper pre-write bounds checks.

### Rationale

**A3:** Why does Pintos implement `strtok_r()` but not `strtok()`?

I believe the important distinction is that `strtok ()` keeps its parsing state internally, while `strtok_r ()` lets each caller keep that state in its own `save_ptr`. If two threads parse different command lines and get scheduled between calls, regular `strtok ()` can mix up those two parsing sequences.

In this implementation, the command line is copied into a separate page before the child parses it. `strtok_r ()` doesn't lock that page or prevent another thread from modifying it; it avoids sharing the tokenizer's internal state. Those are two different problems.

**A4:** In Pintos, the kernel separates commands into executable name and arguments. In Unix-like systems, the shell does this separation. Identify two advantages of the Unix approach.

The approach adopted by Unix-like Operating Systems means that the Shell parses the command line before making the request to the kernel. I see two advantages to this:

1. Quoting, escaping, wildcard expansion, and other command syntax can be handled by the Shell. The kernel doesn't have to understand every shell's parsing rules.
2. The kernel can receive an already separated argument vector. This keeps the interface simpler and lets different shells offer different syntax without changing the process loader.

Of course, the kernel still has to validate and copy the user-provided strings. Moving parsing to the Shell doesn't remove the need to protect against bad pointers or concurrent calls. In this repository, syscall interrupts are registered with INTR_ON, so the handler doesn't run with interrupts disabled throughout.

---

## System Calls

### Data Structures

**B1:** Copy here the declaration of each new or changed `struct`, global or static variable, `typedef`, or enumeration. Identify the purpose of each in 25 words or less.

All the I/O devices including the Disk and Input/Output terminals are accessed by a USER process by TRAPping the CPU and requesting the appropriate system call. The interrupt uses `vec_no` 48 (`0x30`). I kept the individual `sys_*` handlers and auxiliary functions `static` so they belong to `userprog/syscall.c`. This controls which source files can refer to those functions; the actual privilege boundary is enforced by the CPU and the syscall/user-memory handling.

The file also owns a struct representing an opened file:

```c
typedef int pid_t;
typedef int fd_t;

typedef struct proc_file
{
  struct file *file;            /* Reference to the actual file. */
  fd_t file_fd;                 /* Descriptor used by the USER process. */
  struct list_elem thread_elem; /* Entry in the current thread's files list. */
} proc_file;

static struct lock file_system_lock;
```

I defined `allocate_fd ()` and `proc_file_by_fd ()` to create descriptor numbers and find their associated records. `allocate_fd ()` keeps a static `next_fid`, initially 2. Each record is allocated as a whole page using `palloc_get_page (0)`.

The actual filesystem is a shared resource. When we account for scheduling and instruction interleaving, different USER processes can enter filesystem operations unless we synchronize them. The filesystem lock is initialized in `syscall_init ()` and used by the syscall handlers. It is also held across `process_execute ()` in `sys_exec ()`. However, executable loading and process cleanup don't themselves consistently acquire this lock, so synchronization is incomplete.

The small coursework changes to `lib/user/syscall.c` and `lib/user/syscall.h` were not copied. The current base already has the include and compatible declarations; adding a parameter name to `wait ()` doesn't change its interface.

**B2:** Describe how file descriptors are associated with open files. Are file descriptors unique across the entire OS or only within a single process?

File descriptors are looked up within the process. Different processes can open the same actual file, and each open gets its own `struct file` and file position. STDIN and STDOUT are associated with 0 and 1 respectively.

In my implementation, `allocate_fd ()` assigns numbers from one static counter shared by all processes. The numbers therefore happen to be globally allocated, but `proc_file_by_fd ()` searches only `thread_current ()->files`. A process can't use another process's record just by knowing its descriptor number, and the file list isn't inherited by children.

I originally suspected the shared counter was the reason for the 26 failures. Global numbering alone doesn't explain those failures: it is still compatible with per-process lookup. The cleanup bug described below is a concrete problem in the saved implementation.

### Algorithms

**B3:** Describe your implementation for reading and writing user data from the kernel.

Since reading and writing happens through filesystem operations, we acquire `file_system_lock` before accessing an open file. Once the process acquires the lock, we use the functions declared in the filesystem interfaces to perform the operation.

For STDIN, `sys_read ()` calls `input_getc ()` and stores bytes using `put_user ()`. For STDOUT, `sys_write ()` calls `putbuf ()` for the whole buffer. For ordinary descriptors, we find the current process's `proc_file` and call `file_read ()` or `file_write ()`. Writing to STDIN or using a missing descriptor terminates the process in this version.

Here are the two functions as they actually exist in the port:

```c
static int
sys_read (int fd, void *buffer, unsigned length)
{
  is_user_buffer ((const uint8_t *)buffer, length);
  lock_acquire (&file_system_lock);
  int ret;

  /* If STDIN, we simply read using INPUT_GETC (). */
  if (fd == 0) 
  {
    unsigned i;
    for(i = 0; i < length; ++i) 
    {
      if(!put_user (buffer + i, input_getc ())) 
      {
        lock_release (&file_system_lock);
        sys_exit(-1);
      }
    }
    ret = length;
  
  /* Otherwise, we have to read from a file. */
  } else 
  {
    proc_file *f;

    f = proc_file_by_fd (fd);
    if (f == NULL)
    {
      sys_exit (-1);
    }

    if(f && f->file) 
    {
      ret = file_read(f->file, buffer, length);
    
    /* The OS cannot read from it. */
    } else
    {
      ret = -1;
    }
  }

  lock_release (&file_system_lock);
  return ret;
}
```

```c
static int
sys_write (int fd, const void *buffer, unsigned size)
{
  is_user_buffer ((const uint8_t *)buffer, size);
  lock_acquire (&file_system_lock);
  int ret;

  /* Cannot write to STDIN. */
  if (fd == 0)
  {
    sys_exit (-1);
  }

  /* If STDOUT, we simply write to it using PUTBUF (). */
  if(fd == 1) 
  {
    putbuf ((char *)buffer, size);
    ret = size;

  /* Otherwise, we have to write to a file. */
  } else
  {
    proc_file *f;

    f = proc_file_by_fd (fd);
    if (f == NULL)
    {
      sys_exit (-1);
    }

    if (f && f->file) 
    {
      ret = file_write(f->file, buffer, size);
    
    /* The OS cannot WRITE to the file. */
    } else
    {
      ret = -1;
    }
  }

  lock_release (&file_system_lock);
  return ret;
}
```

Of course, calling `is_user_buffer ()` isn't enough by itself. As you can see, the return value is ignored in both functions. Invalid buffers can still reach the filesystem or console operation. Also, probing a buffer with `get_user ()` checks reading, not whether the destination of a read is writable. This is a known limitation of the port, not a completed memory-validation design.

**B4:** Consider a system call copying a full page (4,096 bytes) from user to kernel. What are the least and greatest possible numbers of page table inspections (`pagedir_get_page()` calls)? Repeat this for copying only 2 bytes. Can these numbers be improved?

If 4KB of contiguous data needs to be copied, it occupies one virtual page if aligned, or at most two if it crosses a page boundary. The same is true for 2B: both bytes can be in one page, or one byte can be in each of two pages.

That means a page-by-page validation approach can check a valid range with one or two `pagedir_get_page ()` calls. A naive byte-by-byte page-table check could make 4096 calls for a 4KB range, or two for a 2B range. Early failure can stop the checks before reaching those totals.

However, my saved implementation doesn't call `pagedir_get_page ()` from these validation helpers. It probes bytes using `get_user ()` and relies on the hardware's memory translation and the page-fault recovery path. Those hardware translations are not the same as calls to `pagedir_get_page ()`. `is_user_buffer ()` performs up to 4096 byte probes for 4KB, or two for 2B. The one-or-two-page answer describes a possible improvement using explicit page validation, not the number of explicit calls made by this code.

**B5:** Briefly describe your implementation of the "wait" system call and its interaction with process termination.

Since Pintos requires that a USER process can only wait on processes it spawned, `sys_wait ()` directly calls `process_wait ()` with the given PID. The latter finds the child using `thread_by_tid ()`, verifies its parent, and checks `parent_waiting`.

```c
int
process_wait (tid_t child_tid UNUSED)
{
  struct thread *t, *cur;

  cur = thread_current ();
  t = thread_by_tid (child_tid);

  /* This function is straightforward to implement. We just
     have to ensure that the caller is indeed the parent
     of TID thread running the user-process owned by the
     caller. More detailed description is provided in the
     documentation for WAIT () system call description (section
     3.3.4) */
  if (t == NULL || t->parent != cur || t->parent_waiting)
    return RETURN_ERROR;
  else if (t->return_status != RETURN_INIT || t->exited == true)
    return t->return_status;


  sema_down (&t->sema_exit);
  t->parent_waiting = true;
  sema_down (&t->sema_child);
  sema_up (&t->sema_exit);

  int ret = t->return_status;
  return ret;
}
```

If the thread is found and its `return_status` differs from RETURN_INIT, or `exited` is true, the status is returned immediately. Otherwise the caller acquires `sema_exit`, sets `parent_waiting`, and waits on `sema_child`. `process_exit ()` wakes current waiters on `sema_child` before trying to acquire `sema_exit` itself.

The intended idea is straightforward, but the actual lifetime handling isn't complete. `thread_by_tid ()` only finds live threads, doesn't protect its traversal from interrupts, and doesn't keep the returned object alive. Once a child is removed from ALL_LIST and its page is freed, the parent loses its status. The early-return path also doesn't record that a wait has already happened. Passing the supplied wait tests doesn't cover all those cases.

**B6:** Describe your strategy to handle errors during user memory access in system calls, ensuring primary functions remain clear and resources (locks, buffers) are freed upon error. Provide an example.

Whenever a USER process TRAPs the CPU, the interrupt frame gives `syscall_handler ()` the user's saved stack pointer. The handler runs in kernel context and can therefore access memory that the USER process itself isn't allowed to access. We have to be careful about what we dereference.

To keep the individual syscall bodies readable, I defined five auxiliary functions: `get_user ()`, `put_user ()`, `is_user_word ()`, `is_user_string ()`, and `is_user_buffer ()`. The first two come from the Pintos documentation's fault-recovery approach. The others use byte probes to inspect words, strings, and buffers.

The general idea is as follows:

1. Read the syscall number from ESP using `is_user_word ()`.
2. Based on that number, read up to three argument words. String arguments are also checked with `is_user_string ()`.
3. If those checks fail, call `sys_exit (-1)`.

For WRITE, the handler extracts fd, buffer, and size, then calls `sys_write ()`. The saved function starts with this call:

```c
is_user_buffer ((const uint8_t *)buffer, size);
```

My coursework document also showed a version that checked for `-1` and called `sys_exit (-1)`. That was the intended behavior, but it isn't what the saved source does: the return value is ignored.

There are further gaps. The byte probes don't first enforce the required boundary below PHYS_BASE, and the buffer loop doesn't safely handle address arithmetic overflow. `is_user_string ()` returns `string_length - 1` at the terminator, which means an empty string returns `-1` too. The page-fault handler recovers from every kernel-mode page fault by changing EIP to EAX; it doesn't distinguish the intended probe from another kernel fault.

For cleanup, `sys_exit ()` releases `file_system_lock` if held, closes open files, records status, and calls `thread_exit ()`. However, its loop calls `sys_close ()`, which already removes and frees the `proc_file`, and then removes/frees it again. That is the double-free behind several allocator panics. Exceptions that call `thread_exit ()` directly also bypass this syscall cleanup path. So the intended centralized error handling is there, but safely recovering all resources still needs work.

### Synchronization

**B7:** How does your code ensure the "exec" system call returns -1 only after loading failure is confirmed? How is load success/failure status communicated to the calling thread?

This can be better explained using the control flow. The handler first reads the EXEC argument word and checks the command-line string. `sys_exec ()` then acquires `file_system_lock` and calls `process_execute ()`.

The starter loader already creates a new thread whose initial function is `start_process ()`. The PARENT has to know when that child has finished its loading attempt before returning from EXEC. I used the child's `sema_child` for that notification:

```c
struct thread *child = thread_by_tid (tid);
if (child == NULL)
  return tid;
sema_down (&child->sema_child);
return tid;
```

The child calls `sema_up (&cur->sema_child)` after successful load and stack setup. It also signals the same semaphore when loading fails, before exiting.

Of course, this is synchronization, not a transfer of ownership of the child's memory page. More importantly, both paths send the same notification and no separate load-success value is passed back. After thread creation succeeds, `process_execute ()` returns its TID even if loading failed. It returns `-1` on failures such as command-line allocation or thread creation, but not reliably on executable load failure. This explains the failing `exec-missing` test.

The lookup and the notification also depend on the child thread remaining alive. Keeping a separate load-result record would be necessary to make this safe across scheduling and exit. The current filesystem lock is held while the parent waits, so any revision that makes the loader acquire that same lock has to change this control flow too.

**B8:** Consider parent process `P` and child process `C`. Explain synchronization mechanisms and how race conditions are avoided when `P` calls `wait(C)` before and after `C` exits. How do you ensure resources are freed? Address the special cases when `P` terminates without waiting before or after `C` exits.

Because the child USER process is managed by a separate kernel thread, its execution can be interleaved with the PARENT. The intended coordination is the one described in B5: check `parent`, acquire `sema_exit`, mark `parent_waiting`, and wait for a notification on `sema_child`.

If P waits before C exits, C's `process_exit ()` signals any current waiters on `sema_child`, then acquires `sema_exit` and clears `parent_waiting`. P signals `sema_exit` after waking and reads the status. However, signaling is done before C finishes destroying its page directory and sets `exited`, so notification isn't a guarantee that all cleanup is complete.

If C has already exited, the status is accessible only while C's thread object is still present. Once that object has been removed and freed, `thread_by_tid ()` returns NULL and `wait ()` returns `-1`. The implementation doesn't keep an independent record of an exited child, so it doesn't fully satisfy waiting after exit.

If P exits without waiting, C keeps running and isn't adopted by another process. But its `parent` pointer can now refer to freed memory. The children list isn't used to manage these lifetimes or release child records. The code doesn't fully account for parent-first or child-first termination.

Resource cleanup also remains incomplete: the syscall exit path has the double-free described in B6, direct exception exits bypass that path, and `process_exit ()` allows writes to `cur->exec` without closing the retained executable handle. Load closes its original executable handle, and `start_process ()` reopens it for write denial, leaving a gap between those operations.

The initial process is still waited on by the provided `threads/init.c` code:

```c
#ifdef USERPROG
  process_wait (process_execute (task));
#else
  run_test (task);
#endif
```

For other USER processes, waiting for children is optional. The existing initial-process call is useful, but doesn't by itself solve the status and lifetime problems above.

### Rationale

**B9:** Explain your choice of kernel user-memory access implementation.

I think it is primarily down to security and good abstraction policies. USER processes should run as though they are the only ones running on the system. Their address spaces can contain multiple mapped pages, but they shouldn't access kernel memory or unmapped memory just by passing a pointer to a syscall.

Since a kernel thread has access to the system's resources, failing to verify a USER request can provide access that the USER process itself shouldn't have. I chose the fault-recovery method because it lets the CPU report an invalid access and lets the helper return an error so the syscall can terminate the process and clean up.

Of course, that only works if we check the user/kernel boundary, recover from the intended access instructions, and actually handle the returned error. This version doesn't consistently do those things. The design is the intended abstraction; the failed tests show where the saved implementation still falls short.

**B10:** What advantages or disadvantages exist in your file descriptor design?

The advantage is that each USER process owns a simple list of its open files. Lookup is easy to understand, and separate opens have separate file positions. A child doesn't inherit the parent's list.

I originally guessed that the static `next_fid` counter caused the 26 failures because all processes receive numbers from the same pool. The counter really is shared, but `static` on the function only controls linkage. The descriptor lookup still checks the current process's own list, so global numbering alone isn't the reason those tests fail.

The disadvantages are that lookup walks the entire list, each small `proc_file` consumes a whole page, and the counter has no overflow handling. The exit path also frees the records twice, which is an actual bug rather than a design tradeoff. If I were refining this part, I would first fix ownership and cleanup, then consider a smaller allocation and per-process descriptor management.

**B11:** The default mapping of `tid_t` to `pid_t` is identity. If you changed it, what benefits does your approach offer?

No, I maintained a 1:1 mapping between tid_t and pid_t because I wanted to take advantage of this mapping to implement `sys_exec ()` and `sys_wait ()` system calls.

---

## Survey Questions (Optional)

These questions were not answered in my coursework document.
