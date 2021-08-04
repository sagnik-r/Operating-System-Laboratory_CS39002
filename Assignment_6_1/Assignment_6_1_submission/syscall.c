#include "userprog/syscall.h"
#include "userprog/process.h"
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


struct child_element* get_child_element(tid_t tid,struct list *mylist);
static void syscall_handler (struct intr_frame *);
int write (int fd, const void *buffer_, unsigned size);
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
    lock_init(&stdout_lock);
	lock_init(&file_lock);
}


void handle_exec_exit(struct intr_frame *f, int syscall_number, void *args)
{
	int argv = *((int*) args);
	args += 4;
	
	if(syscall_number == SYS_EXIT)
	{
		exit(argv);
	}
	else if(syscall_number == SYS_EXEC)
	{
		check_valid_ptr((const void*) argv);
        	f -> eax = exec((const char *)argv);
	}	
}

handle_write (struct intr_frame *f, int choose, void *args)
{
    int fd = *((int*) args);
    args += 4;
    int buffer = *((int*) args);
    args += 4;
    int size = *((int*) args);
    args += 4;

    check_valid_ptr((const void*) buffer);
    void * temp = ((void*) buffer)+ size ;
    check_valid_ptr((const void*) temp);
    if (choose == SYS_WRITE)
    {
        f->eax = write (fd,(void *) buffer,(unsigned) size);
    }
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
    case SYS_EXIT:                   /* Terminate process. */  
    case SYS_EXEC:                   /* Execute another process. */
	handle_exec_exit (f, syscall_number, args); 
        break;
    case SYS_WRITE:                  /* Write to STDOUT.*/ 
        handle_write(f, SYS_WRITE,args);
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
        child -> cur_status = KILLED;
    else
        child -> cur_status = EXITED;

    thread_exit();
}

tid_t
exec (const char *cmd_line)
{
	// printf("In exec cmd line = %s\n", cmd_line);
    struct thread* parent = thread_current();
    tid_t pid = -1;

    pid = process_execute(cmd_line);// Create a child process of current process

    struct child_element *child = get_child_element(pid,&parent -> child_list);
    sema_down(&child-> child_thread -> wait_load);
    if(!child -> load_status)// Child failed to load executable file
    {
        return -1;
    }
    return pid;
	
}

int write (int fd, const void *buffer_, unsigned size)
{
    uint8_t * buffer = (uint8_t *) buffer_;
    int ret = -1;
	//printf("Buffer in write = ");
	/*int i;	
	for(i=0; i<size; i++)
	{
		printf("%c", *(char *)(buffer_+i));
	}
	printf("\n");*/


    if (fd == 1) // Write to STDOUT
    {
		lock_acquire(&stdout_lock);
        putbuf( (char *)buffer, size);
		lock_release(&stdout_lock);
        return (int)size;
    }
    return ret;
}
