#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

/* The Arguments passed to the thread itself cannot
   be larger than the standard page size of 4KB. */
#define MAX_ARGS_SIZE 4096

static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);
static int set_program_stack (void **esp, char **save_ptr, char* token);

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name)
{
  char *fn_copy;
  tid_t tid;
  char token[16] = {0};       /* Same size as thread.name field */

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load().

     This is because FILE_NAME is currently owned by the caller
     of this function and any new thread running the process
     FILE_NAME would likely modify the buffer (to parse its
     arguments and such).
  */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy (fn_copy, file_name, PGSIZE);

  /* Cannot use strtok_r to tokenize FILE_NAME because it is
     a pointer to a constant string and strtok_r() modifies
     the string.
  */
  for (size_t index = 0; index < 15; index++)
  {
    if (file_name[index] != ' ')
    {
      token[index] = file_name[index];
    } else
    {
      break;
    }
  }

  /* Create a new thread to execute FILE_NAME. */
  tid = thread_create (token, PRI_DEFAULT, start_process, fn_copy);
  if (tid == TID_ERROR)
  {
    palloc_free_page (fn_copy);
    return tid;
  }

  struct thread* child = thread_by_tid(tid);

  // If the new thread already got scheduled and finished execution,
  // we want to return immediately.
  if (child == NULL)
  {
    return tid;
  } else
  {
    /* The thread for the new process is created and scheduled.
      But we want the thread that created the process (referred
      to as PARENT) to own the memory (page) in which the
      process is running. Currently that memory is owned by the
      newly spawned TID thread. By waiting for the ownership
      of this memory, the PARENT will stop its execution until
      TID thread sets the page up for the process FILE_NAME.
    */
    sema_down(&child->sema_child);

    // Now that the caller THREAD owns the CHILD process, we can
    // return successfully.
    return tid;
  }
}

/* A thread function that loads a user process and starts it
   running.

   This is done by allocating a new page for the user process
   FILE_NAME to run in, sets up its stack in accordance to
   x86 specification, registers the page with the CPU's
   page directory table (PDT) and ACTIVATEs it.

   ACTIVATION in this context means everytime the thread calling
   this function gets scheduled in the future, the CPU's reg-
   -isters point to the user-process FILE_NAME's stack instead
   of the Kernel stack (or more broadly speaking the CPU is set
   to use the user-process's virtual memory context instead of
   the thread's virtual memory context).
*/
static void
start_process (void *file_name_)
{
  char *file_name = file_name_;
  struct intr_frame if_;
  bool success;
  struct thread* cur = thread_current();
  char* save_ptr;
  char* token;

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;

  token = strtok_r(file_name, " ", &save_ptr);

  /* Creates a new page for FILE_NAME process, adds the page
     to the CPU's PDT (activation), copies FILE_NAME
     into the page, sets up the memory values in the binary
     according to ELF (Executable Linkable Format) which is
     a POSIX compliant linking format so Linux uses the same
     format. I believe we can check that an executable is
     encoded according to ELF specifications by examining the
     first few bytes of the executable as characters.
  */
  success = load (file_name, &if_.eip, &if_.esp);


  /* If EFL load successful, push the arguments to FILE_NAME's
     stack. */
  if (success && set_program_stack (&if_.esp, &save_ptr, token))
  {
    /* Now that the stack is set, need to make the executable
       as non-modifiable as long as the process is alive.
       Windows OS does this but surprisingly, Linux allows the
      file on disk to be modified. */
    cur->exec = filesys_open (file_name);
    file_deny_write (cur->exec);

    /* We don't need the buffer associated with the file
       name and its arguments anymore. So we FREE it. */
    palloc_free_page (file_name);

    /* Need to release ownership of the memory in which
        user process is running so PARENT thread can own it. */
    sema_up (&cur->sema_child);

  } else
  {

    palloc_free_page (file_name);
    sema_up (&cur->sema_child);
    thread_exit ();
  }
  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

static int
set_program_stack (void **esp, char **save_ptr, char* token)
{
  int args_pushed;
  int argc = 0;
  void* stack_pointer;

  stack_pointer = *esp;

  /* Push the individual string TOKENs on to the user stack.
      The individual TOKENs are pushed LIFO order (i.e, first
      token is at the highest address on the stack. The rest of
      strings are placed sequentially at lower addresses). However,
      their addresses are stored in FILO order (i.e, the address to
      the first token is stored at the lowest address and the
      addresses to the rest of the strings are stored sequentially
      at higher addresses).
  */

  // Had to cast stack_pointer to the respective type everytime something
  // was being pushed on to the stack.
  while (token != NULL)
  {
    size_t len = strlen (token) + 1;
    stack_pointer = (void*) (((char*) stack_pointer) - len);
    strlcpy ((char*)stack_pointer, token, len);

    // Make sure that the argements pushed on to the stack
    // so far don't exceed 4KB (which they won't because
    // these strings were stored in a 4KB page anyway).
    if (PHYS_BASE - stack_pointer > MAX_ARGS_SIZE)
        return 0;

    argc++;
    token = strtok_r (NULL, " ", save_ptr);
  }

  char *last_ptr = (char*) stack_pointer;

  /* Make sure to align the memory to word boundary

      I can't seem to understand why the below commented out code does
      not work as expected. Here I have to align the stack pointer to
      the word boundary. Which means I have to reduce the stack pointer
      by as many bytes as the reminder upon diving by 4.....

      intptr_t diff = 4 - (((intptr_t) stack_pointer) % 4);

      while (!diff)
      {
        stack_pointer = (((char**) stack_pointer) - 1);
        *((char*)(stack_pointer)) = 0;
        diff--;
      }
      stack_pointer = (((char**) stack_pointer) - 1);
  */

  /* This idea came after snooping around the internet (Please find the
     references in the report) and then remembering that for a 4 byte
     word size, the first byte's address will always be a multiple of
     4 (0, 4, 8, 12, ....., 2^32). Which means that this address when
     expressed as binary will always have 0 in the 2 LSBs (ends with 00).
     Since we are going down the address space when pushing data on to
     the stack, if there is indeed a need to align the memory (the page
     was already initialized with 0s when it was created using
     PALLOC_GET_PAGE (0) inside LOAD function), we can simply set the
     last two bits of ESP to 00. We can do that with bit wise AND of the
     ESP and the value 1111 1111 1111 1111 1111 1111 1111 1100 which can
     be represented as a hexadecimal long literal 0xfffffffc.

     I remember solving similar problems in the course on Computer
     Architecture when studying MIPS processor because it uses a similar
     32 bit word size.

     https://stackoverflow.com/questions/62986886/assuring-stack-pointer-alignment-using-bitwise-operators
  */
  stack_pointer = (void*) (((intptr_t) stack_pointer) & 0xfffffffc);

  /* Push null sentinel. */
  stack_pointer = (((char**) stack_pointer) - 1);
  *((char*)(stack_pointer)) = 0;

  // Pushing the pointers to string TOKENs but this
  // time ordered according to x86 specification
  args_pushed = 0;
  while (args_pushed < argc)
  {
    while (*(last_ptr - 1) != '\0')
      last_ptr++;

    stack_pointer = (((char**) stack_pointer) - 1);
    *((char**) stack_pointer) = last_ptr;
    args_pushed++;
    last_ptr ++;
  }

  // Pushing the address of the array of addresses on
  // to the stack.
  char** argv_ptr = (char**) stack_pointer;
  stack_pointer = (((char**) stack_pointer) - 1);
  *((char***) stack_pointer) = argv_ptr;


  // Pushing the value of argc but this time as an
  // integer
  int* argc_ptr = (int*) stack_pointer;
  argc_ptr--;
  *argc_ptr = argc;
  stack_pointer = (void*) argc_ptr;

  // Pushing a fake "return address" on to the stack
  // to be represent the address of the next instruction
  // in _start () function in entry.c program.
  stack_pointer = (((void**) stack_pointer) - 1);
  *((void**)(stack_pointer)) = 0;

  // Updating the value of stack pointer in the thread's
  // user-process context with the updated STACK_POINTER
  *esp = stack_pointer;

  return 1;
}

/* Waits for thread TID to die and returns its exit status.  If
  it was terminated by the kernel (i.e. killed due to an
  exception), returns -1.  If TID is invalid or if it was not a
  child of the calling process, or if process_wait() has already
  been successfully called for the given TID, returns -1
  immediately, without waiting.

  This function will be implemented in problem 2-2.  For now, it
  does nothing. */
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

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;

  // Have to print the exit status to pass test cases
  printf ("%s: exit(%d)\n", cur->name, cur->return_status);

  /*
     1. Free the executable for re-use by some other thread
     2. Wake up any thread waiting to own the process.
     3. Destroy the current process's page directory and switch back
        to the kernel-only page directory.
     4. Wakeup the owner thread if waiting.
  */
  if (cur->exec != NULL)
    file_allow_write (cur->exec);

  while (!list_empty (&cur->sema_child.waiters))
    sema_up (&cur->sema_child);

  sema_down (&cur->sema_exit);
  cur->parent_waiting = false;

  pd = cur->pagedir;
  if (pd != NULL)
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }

  cur->exited = true;
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp)
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL)
    goto done;
  process_activate ();

  /* Open executable file. */
  file = filesys_open (file_name);
  if (file == NULL)
    {
      printf ("load: %s: open failed\n", file_name);
      goto done;
    }

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024)
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done;
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++)
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type)
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file))
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  file_close (file);
  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file)
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
    return false;

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file))
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz)
    return false;

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;

  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0)
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* Get a page of memory. */
      uint8_t *kpage = palloc_get_page (PAL_USER);
      if (kpage == NULL)
        return false;

      /* Load this page. */
      if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
        {
          palloc_free_page (kpage);
          return false;
        }
      memset (kpage + page_read_bytes, 0, page_zero_bytes);

      /* Add the page to the process's address space. */
      if (!install_page (upage, kpage, writable))
        {
          palloc_free_page (kpage);
          return false;
        }

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp)
{
  uint8_t *kpage;
  bool success = false;

  kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  if (kpage != NULL)
    {
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
      if (success)
        *esp = PHYS_BASE;
      else
        palloc_free_page (kpage);
    }
  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}
