#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

struct child_element* get_child_element(tid_t tid,struct list *child_list);
tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
void split_and_insert (char *token, int *total_length, void **stack_pointer, int *num_args);
void add_word_align (int *word_align, int *total_length, void **stack_pointer);
void add_null_char (void **stack_pointer);
void add_argument_address (int num_args, void **stack_pointer, char **args_pointer);
void add_char_pointers (void **stack_pointer);
void add_number_of_arguments (void **stack_pointer, int num_args);
void add_return_address (void **stack_pointer, void **esp);
#endif /* userprog/process.h */
