#include "userprog/syscall.h"
#include "userprog/process.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "devices/shutdown.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "filesys/off_t.h"
#include "devices/input.h"
#include "lib/kernel/stdio.h"

/* Process identifier. */
typedef int pid_t;

/* File Identifier. */
typedef int fd_t;

/* We want to define a type representing information
   about the file being used by the USERPROCESS. */
typedef struct proc_file
{
  struct file *file;              /* Reference to the actual file on the filesystem. */
  fd_t file_fd;                   /* File Descriptor of the file being used by the USERPROCESS. */
  struct list_elem thread_elem;   /* To include in the list PARENT_THREAD->files. */
} proc_file;

static void syscall_handler (struct intr_frame *f);

/* Naming the equivalent system calls executed by PINTOS thread as
   sys_<name>. For example, syscall_handler () will call the function
   SYS_WRITE () whenever a USERPROG calls the system call procedure
   WRITE (). */
static void sys_halt (void);
static void sys_exit (int status);
static pid_t sys_exec (const char *file);
static int sys_wait (pid_t pid);
static bool sys_create (const char *file, unsigned initial_size);
static bool sys_remove (const char *file);
static int sys_open (const char *file);
static int sys_filesize (int fd);
static int sys_read (int fd, void *buffer, unsigned length);
static int sys_write (int fd, const void *buffer, unsigned size);
static void sys_seek (int fd, unsigned position);
static unsigned sys_tell (int fd);
static void sys_close (int fd);

/* Declaring auxiliary functions used by the OS to manage
   a shared file system. */
static fd_t allocate_fd (void);
static proc_file * proc_file_by_fd (int fid);

/* Declaring auxiliary functions to check for user process
   memory violations. */
static int get_user (const uint8_t *uaddr);
static bool put_user (uint8_t *udst, uint8_t byte);
static bool is_user_word (const uint8_t *uaddr, void *dst);
// static bool is_user_word_alt (const uint8_t *uaddr, void *dst);
static int is_user_string (const uint8_t *uaddr);
static int is_user_buffer (const uint8_t *uaddr, unsigned int length);



/* Instantiating a lock to represent the file system.
   The idea is that only one THREAD/USERPROCESS should
   be able to access the file system at any time.*/
static struct lock file_system_lock;

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");

  lock_init (&file_system_lock);
}

static void
syscall_handler (struct intr_frame *f)
{
  int syscall_num;
  /* First we need to check if the address pointed by the ESP register is
     a valid USER PROCESS memory. */
  if (!is_user_word ((const uint8_t *)(f->esp), (void *)(&syscall_num)))
  {
    sys_exit (-1);
  }

  /* The interrupt frame's esp value tells us where the
     exception originated. */
  switch (syscall_num)
  {
    case SYS_HALT:
    {
      /* HALT () requires no arguments, hence it can be called
         without validating any more memory locations. */
      sys_halt ();
      break;
    }

    case SYS_EXIT:
    {
      int status;
      /* EXIT () requires one argument. Namely, the status
         which is stored in the word at (ESP + 4). */
      if (!is_user_word ((const uint8_t *)(f->esp + 4), (void *)(&status)))
      {
        sys_exit (-1);
      }
      sys_exit (status);
      break;
    }

    case SYS_EXEC:
    {
      char *file_name;
      /* EXEC () requires one argument. Namely, the string
         representing the arguments to be passed to the newly
         created USER PROCESS. */
      if (!is_user_word ((const uint8_t *)(f->esp + 4), (void *)(&file_name)) ||
          is_user_string ((const uint8_t *)file_name) == -1)
      {
        sys_exit (-1);
      }
      f->eax = (int) sys_exec ((const char *)file_name);
      break;
    }

    case SYS_WAIT:
    {
      pid_t pid;
      /* WAIT () requires one argument. Namely, the process ID
         which is stored in the word at (ESP + 4). */
      if (!is_user_word ((const uint8_t *)(f->esp + 4), (void *)(&pid)))
      {
        sys_exit (-1);
      }

      f->eax = sys_wait (pid);
      break;
    }

    case SYS_CREATE:
    {
      char *file_name;
      unsigned int initial_size;
      /* CREATE () requires two arguments. Namely, the string
         representing the NAME of the new file created in the
         file system and its initial SIZE. */
      if (!is_user_word ((const uint8_t *)(f->esp + 4), (void *)(&file_name)) ||
          !is_user_word ((const uint8_t *)(f->esp + 8), (void *)(&initial_size)) ||
          is_user_string ((const uint8_t *)file_name)== -1)
      {
        sys_exit (-1);
      }
      f->eax = sys_create ((const char *)file_name, initial_size);
      break;
    }

    case SYS_REMOVE:
    {
      char *file_name;
      /* REMOVE () requires one argument. Namely, the string
         representing the name of the FILE to be removed by
         the USER PROCESS. */
      if (!is_user_word ((const uint8_t *)(f->esp + 4), (void *)(&file_name)) ||
          is_user_string ((const uint8_t *)file_name)== -1)
      {
        sys_exit (-1);
      }
      f->eax = sys_remove ((const char *)file_name);
      break;
    }

    case SYS_OPEN:
    {
      char *file_name;
      /* OPEN () requires one argument. Namely, the string
         representing the name of the FILE to be opened by
         the USER PROCESS. */
      if (!is_user_word ((const uint8_t *)(f->esp + 4), (void *)(&file_name)) ||
          is_user_string ((const uint8_t *)file_name)== -1)
      {
        sys_exit (-1);
      }
      f->eax = sys_open ((const char *)file_name);
      break;
    }

    case SYS_FILESIZE:
    {
      int fd;
      /* FILESIZE () requires one argument. Namely, the FD
         of the file whose size needs to be determined. */
      if (!is_user_word ((const uint8_t *)(f->esp + 4), (void *)(&fd)))
      {
        sys_exit (-1);
      }
      f->eax = sys_filesize (fd);
      break;
    }

    case SYS_READ:
    {
      int fd;
      unsigned int length;
      void *buffer;
      /* READ () requires three arguments. Namely, the FD
         of the file that needs to be read, the BUFFER to
         which data is read into and the LENGTH of the buffer. */
      if (!is_user_word ((const uint8_t *)(f->esp + 4), (void *)(&fd)) ||
          !is_user_word ((const uint8_t *)(f->esp + 8), &buffer) ||
          !is_user_word ((const uint8_t *)(f->esp + 12), (void *)(&length))
          /*|| is_user_buffer ((const uint8_t *)buffer, length) == -1*/)
      {
        sys_exit (-1);
      }
      f->eax = sys_read (fd, buffer, length);
      break;
    }

    case SYS_WRITE:
    {
      /* We have to validate the arguments passed to the interrupt,
         we also have to copy the values into the respective arguments to be passed
         on to sys_write. Particularly the address (f->esp + 8) contains the
         address of the first character of the buffer. We need to copy that. */
      int fd;
      unsigned int size;
      void *buffer;
      /* WRITE () requires three arguments. Namely, the FD
         of the file that needs to be written, the BUFFER from
         which data is read and the LENGTH of the buffer. */
      if (!is_user_word ((const uint8_t *)(f->esp + 4), (void *)(&fd)) ||
          !is_user_word ((const uint8_t *)(f->esp + 8), &buffer) ||
          !is_user_word ((const uint8_t *)(f->esp + 12), (void *)(&size))
          /*|| is_user_buffer ((const uint8_t *)buffer, size) == -1*/)
      {
        sys_exit (-1);
      }
      f->eax = sys_write (fd, buffer, size);
      break;
    }

    case SYS_SEEK:
    {
      int fd;
      unsigned int position;
      /* SEEK () requires two arguments. Namely, the FD of the
         file and the POSITION at which the data needs to be fetched. */
      if (!is_user_word ((const uint8_t *)(f->esp + 4), (void *)(&fd)) ||
          !is_user_word ((const uint8_t *)(f->esp + 8), (void *)(&position)))
      {
        sys_exit (-1);
      }
      sys_seek (fd, position);
      break;
    }

    case SYS_TELL:
    {
      int fd;
      /* TELL () requires one argument. Namely, the FD of the
         file which is stored in the word at (ESP + 4). */
      if (!is_user_word ((const uint8_t *)(f->esp + 4), (void *)(&fd)))
      {
        sys_exit (-1);
      }
      f->eax = sys_tell (fd);
      break;
    }

    case SYS_CLOSE:
    {
      int fd;
      /* CLOSE () requires one argument. Namely, the FD of the
         file which is stored in the word at (ESP + 4). */
      if (!is_user_word ((const uint8_t *)(f->esp + 4), (void *)(&fd)))
      {
        sys_exit (-1);
      }
      sys_close (fd);
      break;
    }

    default:
      /* This case should never execute... */
      printf ("system call!\n");
      thread_exit ();
  }
}

/* Implementing the SYSCALLS for Project 2 */

static void
sys_halt (void)
{
  shutdown_power_off ();
}

static void
sys_exit (int status)
{
  struct thread *t;
  struct list_elem *e;

  t = thread_current ();

  /* Release the file_system_lock if held by this process. */
  if (lock_held_by_current_thread (&file_system_lock))
  {
    lock_release (&file_system_lock);
  }

  /* Close all the files opened by the USERPROCESS. */
  for (e = list_begin (&t->files); e != list_end (&t->files);
       e = list_remove (e))
  {
    proc_file *f = list_entry (e, proc_file, thread_elem);
    sys_close (f->file_fd);
    palloc_free_page (f);
  }

  t->return_status = status;
  thread_exit ();
}

static pid_t
sys_exec (const char *file)
{
  lock_acquire (&file_system_lock);
  int ret = process_execute (file);
  lock_release (&file_system_lock);
  return ret;
}

static int
sys_wait (pid_t pid)
{
  return process_wait (pid);
}

static bool
sys_create (const char *file, unsigned initial_size)
{
  bool ret;

  if (file == NULL)
  {
     sys_exit (-1);
  }

  lock_acquire (&file_system_lock);
  ret = filesys_create (file, initial_size);
  lock_release (&file_system_lock);

  return ret;
}

static bool
sys_remove (const char *file)
{
  bool ret;

  if (file == NULL)
  {
     sys_exit (-1);
  }

  lock_acquire (&file_system_lock);
  ret = filesys_remove (file);
  lock_release (&file_system_lock);

  return ret;
}

static int
sys_open (const char *file)
{
  if (file == NULL)
  {
    return -1;
  }

  struct file *sys_file;
  proc_file *f;

  f = palloc_get_page (0);
  if (f == NULL)
  {
    return -1;
  }

  // Opening the file system...
  lock_acquire (&file_system_lock);

  sys_file = filesys_open (file);
  if (sys_file == NULL)
  {
    palloc_free_page (f);
    lock_release (&file_system_lock);
    return -1;
  }

  f->file = sys_file;
  f->file_fd = allocate_fd ();
  list_push_back (&thread_current ()->files, &f->thread_elem);

  lock_release (&file_system_lock);

  return f->file_fd;
}

static int
sys_filesize (int fd)
{
  proc_file *f;
  off_t size = -1;

  f = proc_file_by_fd (fd);
  if (f == NULL)
  {
     sys_exit (-1);
  }

  lock_acquire (&file_system_lock);
  size = file_length (f->file);
  lock_release (&file_system_lock);

  return size;
}

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

static void
sys_seek (int fd, unsigned position)
{
  proc_file *f;

  f = proc_file_by_fd (fd);
  if (f == NULL)
  {
     sys_exit (-1);
  }

  lock_acquire (&file_system_lock);
  file_seek (f->file, position);
  lock_release (&file_system_lock);
}

static unsigned
sys_tell (int fd)
{
  proc_file *f;
  off_t pos_offset;

  f = proc_file_by_fd (fd);
  if (f == NULL)
  {
     sys_exit (-1);
  }

  lock_acquire (&file_system_lock);
  pos_offset = file_tell (f->file);
  lock_release (&file_system_lock);

  return pos_offset;
}

static void
sys_close (int fd)
{
  proc_file *f;

  f = proc_file_by_fd (fd);
  if (f == NULL)
  {
     sys_exit (-1);
  }

  lock_acquire (&file_system_lock);
  list_remove (&f->thread_elem);
  file_close (f->file);
  palloc_free_page (f);
  lock_release (&file_system_lock);
}

/* ---------------------------------------------------------------------------------------------------------------------------- */

/* Reads a byte at user virtual address UADDR.
UADDR must be below PHYS_BASE.
Returns the byte value if successful, -1 if a segfault
occurred. */
static int
get_user (const uint8_t *uaddr)
{
int result;
asm ("movl $1f, %0; movzbl %1, %0; 1:"
: "=&a" (result) : "m" (*uaddr));
return result;
}

/* Writes BYTE to user address UDST.
UDST must be below PHYS_BASE.
Returns true if successful, false if a segfault occurred. */
static bool
put_user (uint8_t *udst, uint8_t byte)
{
int error_code;
asm ("movl $1f, %0; movb %b2, %1; 1:"
: "=&a" (error_code), "=m" (*udst) : "q" (byte));
return error_code != -1;
}

/* Reads a WORD starting at UADDR and returns TRUE if the
   entire WORD is part of the USER PROCESS virtual memory.
   Returns FALSE otherwise. */
static bool
is_user_word (const uint8_t *uaddr, void *dst)
{
  uint8_t *index;
  int32_t byte_value;
  size_t count = 0;
  for (index = (uint8_t *)uaddr; index < uaddr + 4; index++)
  {
    if ((byte_value = get_user ((const uint8_t *)index)) < 0)
    {
      return false;
    } else
    {
      *((char *)(dst + count)) = byte_value;
      count++;
    }
  }

  return true;
}

/* Reads a string starting at UADDR and returns the length of the
   string if it is entirely (including the NULL terminator) part
   of the USER PROCESS virtual memory. Returns -1 otherwise. */
static int
is_user_string (const uint8_t *uaddr)
{
  uint8_t *index = (uint8_t *)uaddr;
  int string_length = 0;

  while (get_user((const uint8_t *)index) != -1)
  {
    if (*((char *)index) == '\0')
    {
      return string_length - 1;
    } else
    {
      string_length++;
      index++;
    }
  }

  return -1;
}

/* Reads a BUFFER starting at UADDR and returns its LENGTH
   if it is entirely part of the USER PROCESS virtual memory.
   Returns -1 otherwise. */
static int
is_user_buffer (const uint8_t *uaddr, unsigned int length)
{
  uint8_t *index;
  unsigned int buffer_length = 0;

  for (index = (uint8_t *)uaddr; index < uaddr + length; index ++)
  {
    if (get_user((const uint8_t *)index) != -1)
    {
      buffer_length++;
    } else
    {
      return -1;
    }
  }

  return buffer_length;
}

/* Allocate a new FD for a file. We want the file_descriptor numbers
   to persist as long as Pintos is running. Thus we declare the variable
   NEXT_FID as static. */
static fd_t
allocate_fd (void)
{
  /* fd 0 and fd 1 are reserved for STDIN and STDOUT. */
  static fd_t next_fid = 2;

  return next_fid++;
}

/* Returns the file with the given fid from the current thread's files */
static proc_file *
proc_file_by_fd (int fid)
{
  struct list_elem *e;
  struct thread *t;

  t = thread_current();
  for (e = list_begin (&t->files); e != list_end (&t->files);
       e = list_next (e))
  {
    proc_file *f = list_entry (e, proc_file, thread_elem);
    if (f->file_fd == fid)
      return f;
  }

  return NULL;
}
