#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
struct child_element* get_child_element(tid_t tid, struct list *mylist);
void close_all(struct list *fd_list);
void split_and_insert (char *token, int *total_length, void **stack_pointer, int *num_args);
void add_word_align (int *word_align, int *total_length, void **stack_pointer);
void add_null_char (void **stack_pointer);
void add_argument_address (int num_args, void **stack_pointer, char **args_pointer);
#endif /* userprog/process.h */
