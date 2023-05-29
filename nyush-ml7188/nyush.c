/*
Name: Michelle Lin 
netid: ml7188
CITATIONS:
1. Redirecting standard output in C: https://www.youtube.com/watch?v=5fnVr-zH-SE    
I used this video to learn how to write the below functions:
file_input_redirect
file_output_redirect
file_append
*/
#include <stdio.h> 
#include <unistd.h> 
#include <signal.h> 
#include <string.h> 
#include <setjmp.h>
#include <stdlib.h>
#include <dirent.h> 
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h> 
#include <fcntl.h> 

#define MAXARGS 10
#define MYSH_TOK_BUFFER_SIZE 64

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>";

void run();
static jmp_buf env;

void sighandler(int signum) //signal handler for the three signals
{
    if (!setjmp(env))
    {   
        run();
    }
}

char *builtin_commands_list[] = 
{
    "cd",
    "exit",
    "jobs",
    "fg"
};

void file_output_redirect(char *file_name)
{
    int file = open(file_name, O_WRONLY | O_CREAT | O_TRUNC, 0777); 
    if (file == -1) 
    {
        fprintf(stderr, "Error: file open unsuccessful\n");
        exit(-1);
    }
    dup2(file, 1); //redirect stdout to file
    close(file);
}

void file_input_redirect(char *file_name)
{
    int file = open(file_name, O_RDONLY, 0666);
    if (file == -1)
    {
        fprintf(stderr, "Error: file open unsuccessful\n");
        exit(-1);
    }
    dup2(file, 0);
    close(file);
}

void file_append(char *file_name)
{
    int file = open(file_name, O_RDWR|O_APPEND, 0777);
    if (file == -1)
    {
        fprintf(stderr, "Error: file open unsuccessful\n");
        exit(-1);
    }
    dup2(file, 1);
    close(file);
}

char **redirections_parse(char **args)
{
    int i, j;
    char *file_name = NULL;

    while(args[i])
    {
        //if those variables == 0, it means yes redirection
        int output_redirect_or_not = strcmp(args[i],">");
        int input_redirect_or_not = strcmp(args[i],"<");
        int append_redirect_or_not = strcmp(args[i],">>");

        if(output_redirect_or_not == 0) //output redirection
        {
            file_name = args[i+1];
            file_output_redirect(file_name);
        }
        if(input_redirect_or_not == 0) //input redirection
        {
            file_name = args[i+1]; 
            file_input_redirect(file_name); 
        }
        if(append_redirect_or_not == 0) //append redirection
        {
            file_name = args[i+1];
            file_append(file_name);
        }
        i++; 
    }

    while (args[j])
    {
        //if those variables == 0, it means yes redirection
        int output_redirect_or_not = strcmp(args[j],">");
        int input_redirect_or_not = strcmp(args[j],"<");
        int append_redirect_or_not = strcmp(args[j],">>");

        if(output_redirect_or_not == 0)
        {
            args[j] = NULL;
            break;
        } 
        if(input_redirect_or_not == 0)
        {
            args[j] = NULL;
            break;
        }
        if(append_redirect_or_not == 0)
        {
            args[j] = NULL;
            break;
        }
        j++;
    }
    return args;
}

int execute_checker(char **args) //**args = **tokens
{
    // int i;
    if(args[0] == NULL) 
    {
        return 1; //Case 1: if there is no command
    }

    for (int i = 0; i < 4; i ++) //*** we only have 2 built in functions right now
        if(strcmp(args[0], builtin_commands_list[i]) == 0) //Case 2: if the first command is one of the built in functions
        {
            if (i == 0) //when the command is "cd"
            {
                
                if(args[1] == NULL || args[2] != NULL)
                {
                    fprintf(stderr, "Error: invalid command\n");
                }
                else
                {
                    if(chdir(args[1]) != 0)
                    {
                        fprintf(stderr, "Error: invalid directory\n");
                    }    
                }
                return 1; //cd executed successfully
            }
            else if (i == 1) //when the command is "exit"
            {
                if (args[1] != NULL) //when there are commands when exit is called
                {
                    fprintf(stderr, "Error: invalid command\n");
                    return 1; //ignore
                }
                return 0; //when exit is called successfully
            }
            else if (i == 2) //"jobs"
            {
                if (args[1] != NULL) //when there are commands when exit is called
                {
                    fprintf(stderr, "Error: invalid command\n");
                    return 1; //ignore
                }
            }
            else if (i == 3) //"fg"
            {
                if(args[1] == NULL || args[2] != NULL)
                {
                    fprintf(stderr, "Error: invalid command\n");
                }
                return 1;
            }

        }
    return execute_helper(args); //Case 3: when the command is not built-in and no pipes needed to handle
}

int execute_helper(char **args)
{
    pid_t pid, wpid;
    int status;

    pid = fork();
    if (pid < 0) //Case 2: fork was not successful
    {
        fprintf(stderr, "Fork() unsuccessful\n");
    } 
    else if (pid == 0) //Case 1: We are in the child process
    {
        // if (execvp(args[0], args) == -1) 
        if (execvp((redirections_parse(args))[0], (redirections_parse(args))) == -1)
        {
            fprintf(stderr, "Execution unsuccessful\n");
        }
        exit(EXIT_FAILURE); //EXIT FAILURE (if it ever comes back)
    } 
    else //Case 3: we are in the parent process
    {
        // wait for the child process to finish
        // we wait until either process killed or exited
        do 
        {
            wpid = waitpid(pid, &status, WUNTRACED);
        } 
        while (!WIFEXITED(status) && !WIFSIGNALED(status));
        
    }
    return 1;
}

char** split_line(char *line) //split the command by whitespaces
{
    // int i;
    int buffer_size = MYSH_TOK_BUFFER_SIZE, position = 0;
    char **tokens = malloc(buffer_size * sizeof(char *));
    char *token;

    token = strtok(line, whitespace);
    while(token != NULL)
    {
        tokens[position++] = token;
        token = strtok(NULL, whitespace);
    }

    tokens[position] = NULL;
    return tokens; //pointer to pointer to commands separated by space
}

int get_command(char *buf, int nbuf)
{
    //get the current working directory, save it into path
    char path[100]; 
    getcwd(path, 100); 

    for (int i = strlen(path); i >= 0; i --)
    {
        if (path[i] == '/')
        {
            char now[200] = "[nyush ";
            if (strlen(path) > 1) //if the path is longer than 1 (i.e its not in root directory)
            {
                strcat(now, path + 1 + i); //concatenate "now" with the last path
            }
            else 
            {
                strcat(now, path + i); //concatenate "now" with the root dash
            }
            strcat(now, "]$ "); 
            printf("%s", now); 
            fflush(stdout);
            break;
        }
    }
    
    memset(buf, 0, nbuf); //set buf into all zeros; "000000000000"
    fgets(buf, nbuf, stdin); 
    if(buf[0] == 0)
        return -1;
    return 0;
}
int argument_checker (char **args)
{
    int i;
    int input_redirection_count = 0, output_redirection_count = 0;
    for (i = 0; args[i] != NULL; i++)
    {
        if (strcmp(args[i], ">") == 0)
        {
            output_redirection_count++;
        }
        else if (strcmp(args[i], "<") == 0)
        {
            input_redirection_count++;
        }
    }
    if (input_redirection_count > 1 || output_redirection_count > 1)
    {
        return 0;
    }
    return 1;
}

void run()
{
    char buf[100];
    char buk[100];
    char **args;
    int status, score;

    // Three signals to handle
    signal(SIGINT, sighandler); 
    signal(SIGQUIT, sighandler);
    signal(SIGTSTP, sighandler);

    //the things we need to do after we get the command
    while(get_command(buf, sizeof(buf)) >= 0) 
    {
        memcpy(buk, buf, sizeof(buf)); //copy the content of buf to buk
        args = split_line(buk); //split by tokens
        score = argument_checker(args); //check argument grammar is valid
        if (score == 1) //score = 1 is valid, score = 0 is invalid
        {
            status = execute_checker(args); 
            if(status == 1) //Case 1: when cd is called sucessfully or when exit is called with arguments
            {
                continue; 
            }
            else if (status == 0) //Case 2: when exit is called successfully (no commands after exit)
            {   
                exit(0); 
            }
            else // Case 3: not built in functions, use execute_helper
            {
                printf("how did you get here?");
            }
        }
        else //invalid command
        {
            fprintf(stderr, "Error: invalid command\n");
        }
    }
    
    exit(0);
}

int main()
{
    run();
}