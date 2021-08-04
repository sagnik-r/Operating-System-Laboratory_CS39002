#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <stdbool.h>
#include "threads/thread.h"
#include <list.h>
#include "threads/synch.h"


struct lock file_lock;       /*lock an unlock access file with multi thread*/
struct lock stdout_lock;

void syscall_init (void);
void exit (int status);
tid_t exec (const char *cmd_line);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned size);
int write (int fd, const void *buffer, unsigned size);
void close (int fd);
#endif /* userprog/syscall.h */
