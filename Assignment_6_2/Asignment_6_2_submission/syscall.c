#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "process.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "userprog/process.h"

static void syscall_handler (struct intr_frame *);
struct fd_element* get_fd(int fd);
int write (int fd, const void *buffer_, unsigned size);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned size);
void close (int fd);
tid_t exec (const char *cmdline);
void exit (int status);


void check_valid_ptr (const void *pointer)
{
    if (!is_user_vaddr(pointer))
    {
        exit(-1);
    }

    void *check = pagedir_get_page(thread_current()->pagedir, pointer);
    if (check == NULL)
    {
        exit(-1);
    }
}

void
syscall_init (void)
{
    intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
    lock_init(&file_lock);
    lock_init(&stdout_lock);
}

void handle_exit (struct intr_frame *f, int syscall_number, void *args)
{
	int argv = *((int*) args);
    args += 4;

	exit(argv);
}

void handle_exec (struct intr_frame *f, int syscall_number, void *args)
{
	int argv = *((int*) args);
    args += 4;
	check_valid_ptr((const void*) argv);
    f -> eax = exec((const char *)argv);
}

void handle_wait (struct intr_frame *f, int syscall_number, void *args)
{
	int argv = *((int*) args);
    args += 4;

	f -> eax = wait(argv);	
}

void handle_close(struct intr_frame *f, int syscall_number, void *args)
{
    int argv = *((int*) args);
    args += 4;


}

void handle_remove(struct intr_frame *f, int syscall_number, void *args)
{
	int argv = *((int*) args);
    args += 4;
	check_valid_ptr((const void*) argv);
    f -> eax = remove((const char *) argv);
}

void handle_open(struct intr_frame *f, int syscall_number, void *args)
{
	int argv = *((int*) args);
    args += 4;
	check_valid_ptr((const void*) argv);
    f -> eax = open((const char *) argv);
}

void handle_filesize(struct intr_frame *f, int syscall_number, void *args)
{
	int argv = *((int*) args);
    args += 4;

	f -> eax = filesize(argv);
}

void handle_create(struct intr_frame *f, int syscall_number, void *args)
{
	int argv = *((int*) args);
    args += 4;
    int argv_1 = *((int*) args);
    args += 4;

	check_valid_ptr((const void*) argv);
    f -> eax = create((const char *) argv, (unsigned) argv_1);
}

void handle_write(struct intr_frame *f, int syscall_number, void *args)
{
	int argv = *((int*) args);
    args += 4;
    int argv_1 = *((int*) args);
    args += 4;
    int argv_2 = *((int*) args);
    args += 4;

    check_valid_ptr((const void*) argv_1);
    void * temp = ((void*) argv_1)+ argv_2 ;
    check_valid_ptr((const void*) temp);

	f->eax = write (argv,(void *) argv_1,(unsigned) argv_2);
}

void handle_read (struct intr_frame *f, int syscall_number, void *args)
{
	int argv = *((int*) args);
    args += 4;
    int argv_1 = *((int*) args);
    args += 4;
    int argv_2 = *((int*) args);
    args += 4;

    check_valid_ptr((const void*) argv_1);
    void * temp = ((void*) argv_1)+ argv_2 ;
    check_valid_ptr((const void*) temp);

	f->eax = read (argv,(void *) argv_1, (unsigned) argv_2);
}

static void
syscall_handler (struct intr_frame *f )
{
    int syscall_number = 0;
    check_valid_ptr((const void*) f -> esp);
    void *args = f -> esp;
    syscall_number = *( (int *) f -> esp);
    args+=4;
    check_valid_ptr((const void*) args);
    switch(syscall_number)
    {
    case SYS_EXIT:                   /* syscall exit */
		handle_exit(f, SYS_EXIT,args);
        break;
    case SYS_EXEC:                   /* syscall exec */
		handle_exec(f, SYS_EXEC,args);
        break;
    case SYS_CREATE:                 /* syscall create */
		handle_create(f, SYS_CREATE, args);        
		break;
    case SYS_REMOVE:                 /* syscall remove */
		handle_remove(f, SYS_REMOVE,args);
        break;
    case SYS_OPEN:                   /* syscall open */
		handle_open(f, SYS_OPEN,args);        
		break;
    case SYS_FILESIZE:               /* syscall filesize */
		handle_filesize(f, SYS_FILESIZE,args);        
		break;
    case SYS_READ:                   /* syscall read */
		handle_read(f, SYS_READ, args);        
		break;
    case SYS_WRITE:                  /* syscall write */
		handle_write(f, SYS_WRITE, args);        
		break;
    case SYS_CLOSE:                  /* syscall close */
		handle_close(f, SYS_CLOSE,args);
        break;
	case SYS_WAIT:
		handle_wait(f, SYS_WAIT, args);
		break;
    default:
        exit(-1);
        break;
    }
}

void exit (int status)
{
    struct thread *cur = thread_current();
    printf ("%s: exit(%d)\n", cur -> name, status);

    struct child_element *child = get_child_element(cur->tid, &cur -> parent -> child_list);
    child -> exit_status = status;
    if (status == -1)
    {
        child -> cur_status = KILLED;
    }
    else
    {
        child -> cur_status = EXITED;
    }

    thread_exit();
}

tid_t
exec (const char *cmd_line)
{
    struct thread* parent = thread_current();
    tid_t pid = -1;

    pid = process_execute(cmd_line);

    struct child_element *child = get_child_element(pid,&parent -> child_list);
    sema_down(&child-> child_thread -> wait_load);
    if(!child -> load_status)
    {
        return -1;
    }
    return pid;
}

bool create (const char *file, unsigned initial_size)
{
    lock_acquire(&file_lock);
    bool ret = filesys_create(file, initial_size);// create the file
    lock_release(&file_lock);
    return ret;
}

bool remove (const char *file)
{
    lock_acquire(&file_lock);
    bool ret = filesys_remove(file);// remove the file
    lock_release(&file_lock);
    return ret;
}

int open (const char *file)
{
    int ret = -1;
    lock_acquire(&file_lock);
    struct thread *cur = thread_current ();
    struct file * opened_file = filesys_open(file);
    lock_release(&file_lock);
    if(opened_file != NULL)
    {
        cur->fd_size = cur->fd_size + 1;
        ret = cur->fd_size;
        struct fd_element *file_d = (struct fd_element*) malloc(sizeof(struct fd_element));
        file_d->fd = ret;
        file_d->my_file = opened_file;
        list_push_back(&cur->fd_list, &file_d->element);
    }
    return ret;
}

int filesize (int fd)
{
    struct file *my_file = get_fd(fd)->my_file;
    lock_acquire(&file_lock);
    int ret = file_length(my_file);
    lock_release(&file_lock);
    return ret;
}

int read (int fd, void *buffer, unsigned size)
{
    int ret = -1;
    if(fd == 0)
    { // STDIN input
        ret = input_getc();
    }
    else if(fd > 0)
    {
        struct fd_element *fd_elem = get_fd(fd);
        if(fd_elem == NULL || buffer == NULL)
        {
            return -1;
        }
        struct file *my_file = fd_elem->my_file;
        lock_acquire(&file_lock);
        ret = file_read(my_file, buffer, size);
        lock_release(&file_lock);
        if(ret < (int)size && ret != 0)
        {	// Not able to read the entire file
            ret = -1;
        }
    }
    return ret;
}

int write (int fd, const void *buffer_, unsigned size)
{
    uint8_t * buffer = (uint8_t *) buffer_;
    int ret = -1;
    if (fd == 1)
    {	// STDOUT output
        putbuf( (char *)buffer, size);
        return (int)size;
    }
    else
    {
        struct fd_element *fd_elem = get_fd(fd);
        if(fd_elem == NULL || buffer_ == NULL )
        {
            return -1;
        }
        struct file *my_file = fd_elem->my_file;
        lock_acquire(&file_lock);
        ret = file_write(my_file, buffer_, size);
        lock_release(&file_lock);
    }
    return ret;
}

void close (int fd)
{
    struct fd_element *fd_elem;
    struct list_elem *e;

    for(e = list_begin (&thread_current()->fd_list); e!= list_end(&thread_current()->fd_list); e=list_next(e))
	{
		fd_elem = list_entry (e, struct fd_element, element);
		if(fd_elem->fd == fd)
		{
			struct file *my_file = fd_elem->my_file;
    		lock_acquire(&file_lock);
    		list_remove(e);
    		file_close(my_file);
    		lock_release(&file_lock);

    		break;
		}
	}
}

int wait (tid_t pid)
{
    return process_wait(pid);
}

struct fd_element*
get_fd(int fd)
{
    struct list_elem *e;
    for (e = list_begin (&thread_current()->fd_list); e != list_end (&thread_current()->fd_list);
            e = list_next (e))
    {
        struct fd_element *fd_elem = list_entry (e, struct fd_element, element);
        if(fd_elem->fd == fd)
        {
            return fd_elem;
        }
    }
    return NULL;
}

