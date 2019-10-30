#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h> 
#include <netinet/in.h>
#define READBUF_SIZE 15000
#define TOKENBUF_SIZE 256
#define TOK_DELI " <>"
#define CMD_DELI "|!"
#define FIL_DELI ">"
#define ERR_DELI1 "|"
#define ERR_DELI2 "!"
#define USER_DELI "<>"
#define MAX 80 
#define PORT 8080 
#define SA struct sockaddr


//--Built in functions-----------------------------------------------
int sh_exit(char **args)
{
    exit(0);
}

int sh_printenv(char **args)
{
    char * env = getenv(args[1]);
    if(env)
    {
        printf("%s\n", getenv(args[1]));
    }
    return 1;
}

int sh_setenv(char **args)
{
    setenv(args[1], args[2], 1);
    return 0;
}

char *builtin_str[] = 
{
    "exit",
    "printenv",
    "setenv"
};

int (*builtin_func[]) (char **) = 
{
    &sh_exit,
    &sh_printenv,
    &sh_setenv
};

int num_builtins() 
{
    return sizeof(builtin_str) / sizeof(char *);
}
//------------------------------------------------------------------
struct number_pipe
{
    int target_cmd_num;
    int in_fd;
    int out_fd;
};

char *read_line()
{
    int bufsize = READBUF_SIZE;
    int line_position = 0;
    int c;
    char *buffer = malloc(sizeof(char)*bufsize);
    if(!buffer)
    {
        printf("Allocation Fail.\n");
        exit(EXIT_FAILURE);
    }
    while(1)
    {
        c = getchar();
        if(c == '\n')
        {
            buffer[line_position] = '\0';
            return buffer;
        }
        else if(c == EOF)
        {
            printf("\n");
            exit(0);
        }
        else
        {
            buffer[line_position] = c;
        }
        line_position++;
        if(line_position >= bufsize)
        {
            bufsize += READBUF_SIZE;
            buffer = realloc(buffer, bufsize);
            if(!buffer)
            {
                printf("Allocation Fail.\n");
                exit(EXIT_FAILURE);
            }
        }
    }
}

char **parse_line(char *line, char * delimeters)// Parse line with CMD_DELIMETERS
{
    int bufsize = TOKENBUF_SIZE, position = 0;
    char **tokens = (char **)malloc(sizeof(char*) * bufsize);
    char *token;
    if(!tokens)
    {
        printf("Allocation Fail.\n");
        exit(EXIT_FAILURE);
    }
    token = strtok(line, delimeters);
    while(token != NULL)
    {
        tokens[position] = token;
        position++;
        if(position >= bufsize){
            bufsize += TOKENBUF_SIZE;
            tokens = (char **)realloc(tokens, sizeof(char*) * bufsize);
            if(!tokens)
            {
                printf("Allocation Fail.\n");
                exit(EXIT_FAILURE);
            }
        }
        token = strtok(NULL, delimeters);
    }
    tokens[position] = NULL;
    return tokens;
}

int parse_filepipe(char *line)
{
    int filepipe_idx = -1;
    char *file_line = malloc(sizeof(line)*2000);
    if(!file_line)
    {
        printf("File Line Allocation Fail.\n");
        exit(EXIT_FAILURE);
    }
    strcpy(file_line, line);
    char **filepipe_check0;
    char **filepipe_check1;
    filepipe_check0 = parse_line(file_line, CMD_DELI);
    
    int i = 0;
    while(filepipe_check0[i])
    {
        filepipe_check1 = parse_line(filepipe_check0[i], FIL_DELI);
        if(filepipe_check1[1] && !isdigit(*filepipe_check1[1])){filepipe_idx = i;}
        i++;
    }
    free(file_line);
    return filepipe_idx;
}

int* parse_userpipe(char* line)
{
    static int userpipe_idx[1024];
    for(int i = 0; i < 1024; i++){userpipe_idx[i] = -1;}
    char* user_line = malloc(sizeof(line)*2048);
    if(!user_line)
    {
        printf("User Line Allocation Fail.\n");
        exit(EXIT_FAILURE);
    }
    strcpy(user_line, line);
    char** userpipe_check0;
    char** userpipe_check1;
    userpipe_check0 = parse_line(user_line, CMD_DELI);
    int i = 0;
    int j = 0;
    while (userpipe_check0[i])
    {
        userpipe_check1 = parse_line(userpipe_check0[i], USER_DELI);
        if(userpipe_check1[1] && isdigit(*userpipe_check1[1]))
        {
            userpipe_idx[j] = i;
            j++;
        }
        i++;
    }
    free(user_line);
    return userpipe_idx;
}


int* parse_errorpipe(char *line)
{
    static int err_idx[5000];
    for(int i = 0; i < 5000; i++){err_idx[i] = -1;}// Initialize all to -1
    char *err_line = malloc(sizeof(line)*2000);
    if(!err_line)
    {
        printf("Error Line Allocation Fail.\n");
        exit(EXIT_FAILURE);
    }
    strcpy(err_line, line);
    char **errpipe_check0; 
    char **errpipe_check1;
    int err_idx_cnt = 0; // record how many indexes did err_idx used
    int errpipe_idx = 0; // record the currently checking cmd idx if it's err pipe
    errpipe_check0 = parse_line(err_line, ERR_DELI1);
    while(errpipe_check0[errpipe_idx])
    {
        errpipe_check1 = parse_line(errpipe_check0[errpipe_idx], ERR_DELI2);
        if(errpipe_check1[1])
        {
            err_idx[err_idx_cnt] = errpipe_idx + err_idx_cnt;
            err_idx_cnt++;
        }
        errpipe_idx++;
    }
    free(err_line);
    return err_idx;
}

void remove_spaces(char* s) 
{
    const char* d = s;
    do {
        while (*d == ' ') {
            ++d;
        }
    } while (*s++ = *d++);
}

int check_builtin(char**args)
{
    int builtin_flag = 0;
    for (int i = 0; i < num_builtins(); i++) 
    {
        if (strcmp(args[0], builtin_str[i]) == 0) {
            builtin_flag = 1;
            (*builtin_func[i])(args);// Execute built in command
            break;
        }
    }
    return builtin_flag;
}

void close_unused_fd()
{
    for(int i = 3; i < 1024; i++)
    {
        close(i);
    }
}

int file_exist(char* file)
{
    char bin[20];
    strcpy(bin, "bin/");
    strcat(bin, file);
    if( access( bin, F_OK ) != -1 ) {return 1;} 
    else {return 0;}
}

void childHandler(int signo) 
{
    int status;
    while (waitpid(-1, &status, WNOHANG) > 0) 
    {
        //do nothing
    }
}

// Function designed for chat between client and server. 
void func(int sockfd) 
{ 
    char buff[MAX]; 
    int n; 
    // infinite loop for chat 
    for (;;) { 
        bzero(buff, MAX); 
  
        // read the message from client and copy it in buffer 
        read(sockfd, buff, sizeof(buff)); 
        // print buffer which contains the client contents 
        printf("From client: %s\t To client : ", buff); 
        bzero(buff, MAX); 
        n = 0; 
        // copy server message in the buffer 
        while ((buff[n++] = getchar()) != '\n') 
            ; 
  
        // and send that buffer to client 
        write(sockfd, buff, sizeof(buff)); 
  
        // if msg contains "Exit" then server exit and chat ended. 
        if (strncmp("exit", buff, 4) == 0) { 
            printf("Server Exit...\n"); 
            break; 
        } 
    } 
} 


void shell_loop()
{
    char *line;
    char **cmd;
    char **args;
    int status = 1;
    int cmd_no = 1; //record ?th cmd is executing
    int numpipe_idx = 0;// record numbered pipe list idx
    struct number_pipe pipe_num[2000];// record numbered pipe
    pid_t pid;

    do
    {
        printf("%% ");
        //line = read_line();
        //--Read from client-----------------------------------------

        //-----------------------------------------------------------
        if(!line){break;}

        //--Parse user pipe------------------------------------------
        int* userpipe_idx;
        userpipe_idx = parse_userpipe(line);
        //-----------------------------------------------------------

        //--Parse file pipe------------------------------------------
        int filepipe_idx;
        filepipe_idx = parse_filepipe(line);
        //-----------------------------------------------------------
        
        //--Parse error pipe-----------------------------------------
        int * err_idx_table = parse_errorpipe(line); 
        //-----------------------------------------------------------
        
        //--Parse line and calculate cmd num-------------------------
        cmd = parse_line(line, CMD_DELI);
        int num_of_cmd = 0;
        while(cmd[num_of_cmd])// Calculate number of command
        {
            num_of_cmd++;
        }
        //-----------------------------------------------------------
        
        //--Check if end with pipeN----------------------------------
        int lineEndsWithPipeN = 0;
        int last_cmd_idx = 0;
        while(cmd[num_of_cmd-1][last_cmd_idx])
        {
            last_cmd_idx ++;
        }
        if(isdigit(cmd[num_of_cmd-1][last_cmd_idx-1]))
        {
            lineEndsWithPipeN = 1;
        }
        //-----------------------------------------------------------

        int cmd_idx = 0;
        while(cmd_idx < num_of_cmd)
        {
            //--Check if current cmd has file pipe-------------------
            int is_filepipe = 0;
            char* filename;
            if(cmd_idx == filepipe_idx)
            {
                is_filepipe = 1;
                char** file_tmp = parse_line(cmd[cmd_idx], FIL_DELI);
                filename = file_tmp[1];
                remove_spaces(filename);
            }
            //-------------------------------------------------------
            
            //--Check if current cmd has err pipe--------------------
            int is_errpipe = 0;
            int errpipe_idx = 0;
            while(err_idx_table[errpipe_idx] != -1)
            {
                if(cmd_idx == err_idx_table[errpipe_idx])
                {
                    is_errpipe = 1;
                    break;
                }
                errpipe_idx++;
            }
            //-------------------------------------------------------

            //--Skip last cmd if it's just numpipe(....|2)-----------
            int i = 0;
            while(isdigit(cmd[cmd_idx][i])){i++;}
            if(cmd_idx==num_of_cmd-1 && isdigit(cmd[cmd_idx][0]) && !cmd[cmd_idx][i]){break;}
            //-------------------------------------------------------

            //--Parse cmd argument with space------------------------
            args = parse_line(cmd[cmd_idx], TOK_DELI);
            if (args[0] == NULL) {continue;}// Skip empty cmd.
            args = args + i;// If start with digit skip it.(1 ls)
            //-------------------------------------------------------

            //--Check if it's builtin function-----------------------
            int builtin_flag;
            builtin_flag = check_builtin(args);
            if(builtin_flag)
            {
                cmd_no++;
                cmd_idx++;
                continue;
            }
            //-------------------------------------------------------

            //--Parse numpipe----------------------------------------
            int stdin_fd = STDIN_FILENO;
            int stdout_fd = STDOUT_FILENO;
            int is_numpipe = 0;
            if(cmd[cmd_idx+1])
            {
                if(isdigit(cmd[cmd_idx+1][0])&&cmd[cmd_idx+1][0]!='1'||isdigit(cmd[cmd_idx+1][1]))// record every num cmd
                {
                    is_numpipe = 1;
                    int i = 0;
                    int target_no = 0;
                    while(isdigit(cmd[cmd_idx+1][i]))
                    {
                        target_no = target_no*10 + ((int)cmd[cmd_idx+1][i]-48);
                        i++;
                    }
                    target_no = cmd_no + target_no;
                    int others_target = 0;
                    for(int i = 0; i < numpipe_idx; i++)
                    {
                        if(target_no == pipe_num[i].target_cmd_num)
                        {
                            stdout_fd = pipe_num[i].out_fd;
                            others_target = 1;
                            break;
                        }
                    }
                    if(!others_target)
                    {
                        int fd[2];
                        pipe(fd);
                        pipe_num[numpipe_idx].target_cmd_num = target_no;
                        // start from previous cmd
                        pipe_num[numpipe_idx].in_fd = fd[0];// current cmd's fd
                        pipe_num[numpipe_idx].out_fd = fd[1];
                        stdout_fd = fd[1];
                        // printf("Numpipe recorded | Target No: %d | fd:(%d, %d)\n", \
                        //     pipe_num[numpipe_idx].target_cmd_num, pipe_num[numpipe_idx].in_fd, pipe_num[numpipe_idx].out_fd);
                        numpipe_idx++;
                    }
                }
                else
                {
                    int next_cmd_is_target = 0;
                    //Need to check if next cmd is someone's target.
                    int i;
                    for(i = 0; i < numpipe_idx; i++)
                    {
                        if(cmd_no + 1 == pipe_num[i].target_cmd_num)
                        {
                            // redirect pipe to previous source of next cmd.
                            stdout_fd = pipe_num[i].out_fd;
                            next_cmd_is_target = 1;
                            break;
                        }
                    }
                    if(next_cmd_is_target == 0)// next cmd is not someone's target
                    {
                        int fd[2];
                        pipe(fd);
                        stdout_fd = fd[1];
                        pipe_num[numpipe_idx].in_fd = fd[0];// current cmd's fd
                        pipe_num[numpipe_idx].out_fd = fd[1];
                        pipe_num[numpipe_idx].target_cmd_num = cmd_no + 1;
                        numpipe_idx++;
                    }
                }
                numpipe_idx %= 2000;
            }
            //-------------------------------------------------------

            //--Check if current cmd is target-----------------------
            int is_target = 0;
            int stdin_pipe[2];
    
            for(int i = 0; i<numpipe_idx; i++)
            {
                if(cmd_no == pipe_num[i].target_cmd_num)
                {
                    is_target = 1;
                    stdin_pipe[0] = pipe_num[i].in_fd;
                    stdin_pipe[1] = pipe_num[i].out_fd;
                    stdin_fd = pipe_num[i].in_fd;
                    break;
                }
            }
            //-------------------------------------------------------
            if(is_target){close(stdin_pipe[1]);}
            //--Start forking----------------------------------------

            signal(SIGCHLD, childHandler);
            pid = fork();
            while(pid < 0)
            {
                pid = fork();
                usleep(1000);
            }
            if(pid == 0)
            {
                if(cmd_idx == 0)
                {
                    if(is_target)
                    {
                        dup2(stdin_fd, STDIN_FILENO);
                        if(is_filepipe){stdout_fd = open(filename, O_RDWR|O_CREAT|O_TRUNC, 0666);}
                        if(is_errpipe){dup2(stdout_fd, STDERR_FILENO);}
                        dup2(stdout_fd, STDOUT_FILENO);
                    }
                    else
                    {
                        if(is_filepipe){stdout_fd = open(filename, O_RDWR|O_CREAT|O_TRUNC, 0666);}
                        if(is_errpipe){dup2(stdout_fd, STDERR_FILENO);}
                        if(!dup2(stdout_fd, STDOUT_FILENO))
                        {
                            printf("dup fail\n");
                        }
                    }
                }
                else if(cmd_idx == num_of_cmd-1)
                {
                    dup2(stdin_fd, STDIN_FILENO);
                    if(is_errpipe){dup2(stdout_fd, STDERR_FILENO);}
                    dup2(stdout_fd, STDOUT_FILENO);
                }
                else
                {
                    dup2(stdin_fd, STDIN_FILENO);
                    if(is_filepipe){stdout_fd = open(filename, O_RDWR|O_CREAT|O_TRUNC, 0666);}
                    if(is_errpipe){dup2(stdout_fd, STDERR_FILENO);}
                    if(dup2(stdout_fd, STDOUT_FILENO) < 0)
                    {
                        printf("dup fail\n");
                        exit(0);
                    }
                }
                close_unused_fd();
                if(execvp(args[0], args) == -1)
                {
                    printf("Unknown command: [%s].\n", args[0]);
                }
                exit(0);
            }
            else
            {
                if(is_target){close(stdin_pipe[0]);}
                if(is_filepipe){num_of_cmd-=1;}
                if(!lineEndsWithPipeN && cmd_idx == num_of_cmd-1)
                {
                    int status_child;
                    waitpid(pid, &status_child, 0);
                }
            }
            //-------------------------------------------------------
            cmd_no++;
            cmd_idx++;
        }
        free(line);
        free(cmd);
    } while (status);
}


int main()
{
    setenv("PATH", "bin:.", 1);
    shell_loop();
    return 0;
}
