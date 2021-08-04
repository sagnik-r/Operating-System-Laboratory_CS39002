#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h> 
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define SHELL_BUFSIZE 1024
#define SHELL_TOK_BUFSIZE 64
#define SHELL_TOK_DELIM " \t\r\n\a"
#define SHELL_PIPE_DELIM "|"

int execute(char **args, int position);
char ** shell_split_line(char * line, int *position);
char ** shell_split_pipe(char * line, int *size);
char* shell_read_line();
void shell_loop();
int shell_cd(char **args);
int shell_exit(char **args);
int shell_help(char **args);
int shell_launch(char **args, int position, int in, int out);
int max(int a,int b);


char *built_in[] = 
{
    "cd",
    "exit",
    "help"
};

int (*built_in_func[]) (char **) = 
{
    &shell_cd,
    &shell_exit,
    &shell_help
};

int shell_num_builtins()
{
    return sizeof(built_in) / sizeof(char *);
}

int main(int argc, char **argv)
{

    // Command loop
    shell_loop();

    // shutdown actions
    return EXIT_SUCCESS;
}


void shell_loop() // The shell loop
{
    char *line; // Input line
    size_t line_size = 1024;	//Default buffer size
    char **args;
    char **pipes;
    int status;
    int position, pipe_size;
    line = (char *)(malloc(line_size * sizeof(char)));
    do
    {
        
        printf("--> ");
        line = shell_read_line();
        pipes = shell_split_pipe(line, &pipe_size);
        if (pipe_size == 1) {
            args = shell_split_line(line, &position);
            int i=0;
            status = execute(args, position);
        }
        else if (pipe_size > 1) {
        
            int i;
            pid_t pid;
            // in handles the file descriptor for input
            int in = 0, fd[2];
            for (i=0; i<pipe_size - 1; ++i) {
                pipe(fd);
                args = shell_split_line(pipes[i], &position);
                status = shell_launch(args, position, in, fd[1]);
                close(fd[1]);
                in = fd[0];
            }

            args = shell_split_line(pipes[i], &position);
            status = shell_launch(args, position, in, 1);
        }
        // Cleaning memory
        free(line);
        free(args);
        fflush(stdin);
        fflush(stdout);
    } while (status);

}

char * memory_failed_error()
{
    fprintf(stderr, "Shell: failed to allocate memory for line\n");
    exit(EXIT_FAILURE);
}

char* shell_read_line()
{
    int bufsize = SHELL_BUFSIZE;
    int position = 0;
    char *buffer = malloc(sizeof(char) * bufsize);
    int c;
    
    if (!buffer)
    {
        memory_failed_error();
    }
    while(1)
    {
        c = getchar();

        if (c==EOF || c=='\n')
        {
            buffer[position] = '\0';
            return buffer;
        } 
        else
        {
            buffer[position] = c;
        }
        position++;
        
        // handle buffer limit extension
        if (position >= bufsize)
        {
            bufsize += SHELL_BUFSIZE;
            buffer = realloc(buffer, bufsize);
            if (!buffer){
                memory_failed_error();
            }
        }
    }
}

char** shell_split_line(char* line, int *position)// Function to split the line wrt Shell token delimiter
{
    int bufsize = SHELL_TOK_BUFSIZE;
    *position = 0;

    char **tokens = malloc(sizeof(char*) * bufsize);
    char *token;

    if (!tokens) // Not enough memory to allocate for tokens
    {
        memory_failed_error();
    }

    token = strtok(line, SHELL_TOK_DELIM);// Tokenizing the input line wrt shell delimiter tokens
    while (token != NULL)
    {
        
	//Storing the tokens
        tokens[*position] = token;
        (*position)++;
        
        if ((*position) >= bufsize)
        {
            bufsize += SHELL_TOK_BUFSIZE;
            tokens = realloc(tokens, bufsize * sizeof(char *));
            if (!tokens)
            {
                memory_failed_error();
            }
        }
        token = strtok(NULL, SHELL_TOK_DELIM);
    }
    tokens[(*position)] = NULL;
    
    //Testing the token generating function
    
    /*printf("Printing the tokens : \n");
    for(int i = 0; i<*position; i++)
    {
    	printf("%s\n", tokens[i]);	
    }*/
    
    
    return tokens;
}

char** shell_split_pipe(char* line, int *size) // Function to split line wrt pipe [|] character 
{
    int bufsize = SHELL_TOK_BUFSIZE;
    *size = 0;

    char **tokens = malloc(sizeof(char*) * bufsize);
    char *token;

    if (!tokens) // Not enough memory for tokens
    {
        memory_failed_error();
    }

    token = strtok(line, SHELL_PIPE_DELIM); // Split the line wrt | token
    while (token != NULL)
    {
        tokens[*size] = token;
        (*size)++;
        
        if ((*size) >= bufsize)
        {
            bufsize += SHELL_TOK_BUFSIZE;
            tokens = realloc(tokens, bufsize * sizeof(char *));
            if (!tokens)
            {
                memory_failed_error();
            }
        }
        token = strtok(NULL, SHELL_PIPE_DELIM);
    }
    tokens[(*size)] = NULL;
    
    
    //Test to see if tokens are proper
    
    /*printf("Tokens extracted in pipe : \n");
    for(int i=0; i<*size; i++)
    {
    	printf("%s\n", tokens[i]);
    }*/
    return tokens;
}

int shell_launch(char **args, int position, int in, int out) 
{
    pid_t pid, wpid;
    int status;
    long int size = position;
    int i=0;

    pid = fork();
    if (pid == 0) 
    {

        if (in != 0)
        {
          dup2 (in, 0);
          close (in);
        }

        if (out != 1)
        {
          dup2 (out, 1);
          close (out);
        }
    	// Child process
        int count=0;
        for(i=0;i<size;++i)
        {
            if(strcmp(args[i],"&")==0) count++;// Background process
            if(strcmp(args[i],">")==0)//Output Redirection
            {
                close(1);
                dup(open(args[i+1], O_WRONLY));
                count+=2;
            }
            if(strcmp(args[i],"<")==0)//Input redirection
            {
                close(0);
                dup(open(args[i+1], O_RDONLY));
                count+=2;
            }
        }

        char ** args2 = (char**)malloc(sizeof(char*)*(size-1));
        for(i=0;i<size-count;++i) 
        {
            args2[i] = args[i];
        }
        
        
        /*if (execvp(args2[0], args2) == -1) perror("shell");

        exit(EXIT_FAILURE);*/
        
        execvp(args2[0], args2);
        perror("execvp() failed");
        
        exit(EXIT_FAILURE);
    }
    else if (pid < 0) perror("forking error!");
    else
    {
        // Parent process
    	if(!strcmp(args[size-1],"&")==0)
    	{
    		do 
	        {
	            wpid = waitpid(pid, &status, WUNTRACED);
	        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    	} 
    }

    return 1;
}

int execute(char **args, int position)
{
    if (args[0] == NULL)
    {
        return 1;
    } else
    {
        for (int i = 0; i<shell_num_builtins(); i++)
        {// Check if it is a built in command ( cd , exit , help )
            if (strcmp(built_in[i], args[0]) == 0)
            {
                return (*built_in_func[i])(args);
            }
        }
        return shell_launch(args, position, 0, 1);//Not a built in command
    }
    
}

int shell_cd(char **args)// Change directory
{
    if (args[1] == NULL)
    {
        fprintf(stderr, "shell: expected argument to \"cd\" command!");
    } else
    {
        if (chdir(args[1]) != 0)
        {
            perror("shell");
        }
    }
    return 1;
}

int shell_help(char **args)//Help
{
    printf("Shell help!\n");
    printf("The builtin commands are: \n");
    for (int i = 0; i<shell_num_builtins(); i++)
    {
        printf(" %s\n", built_in[i]);
    }

    printf("Use the man command for knowing about other commands!");
}

int shell_exit(char **args)//Exit
{
    return 0;
}
