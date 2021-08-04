#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <stdbool.h>
#include "threads/thread.h"
#include <list.h>
#include "threads/synch.h"


struct lock stdout_lock;       /*lock an unlock access file with multi thread*/
struct lock file_lock;		/* Lock used while opening files in load */
struct fd_element
{
    int fd;                        /*file descriptors ID*/
    struct file *myfile;           /* the real file*/
    struct list_elem element;      /*list elem to add fd_element in fd_list*/
};


void syscall_init (void);
void exit (int status);
tid_t exec (const char *cmd_line);
int write (int fd, const void *buffer, unsigned size);

#endif /* userprog/syscall.h */
