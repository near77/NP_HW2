#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <cstring>
#include <fstream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/mman.h>

using namespace std;

struct command
{
    vector<string> args;
    string type;
    string file;
    int num_pipe;
    int in_usr_id;
    int out_usr_id;
};

struct number_pipe
{
    int target_cmd_num;
    int in_fd;
    int out_fd;
};

struct client_share_memory
{
    int lock;//init 0
    char client_name[30][200];//init (no name)
    int client_pid[30];//init 0
    int usr_pipe_table[30][30];//init 0, if != 0 ; file_name = (i+1)_(j+1)
    char tell_table[30][30][1024];//init ""
    char ip_port_table[30][50];
    int usr_pipe_fd[30][30];
};


string message = 
"****************************************\n\
** Welcome to the information server. **\n\
****************************************\n\
*** User '(no name)' entered from ";
int client_id = -1;
int exit_flag = 0;

client_share_memory * share_mem = (client_share_memory*)mmap(NULL,
                                    sizeof(client_share_memory), 
                                    PROT_READ|PROT_WRITE, MAP_ANON|MAP_SHARED,
                                    -1, 0);

void printenv(vector <string> args)
{
    string env = getenv(args[1].c_str());
    if(env!=""){printf("%s\n", env.c_str());}
}

void init_share_mem()
{
    share_mem->lock = 0;
    for(int i = 0; i < 30; i++)
    { 
        strcpy(share_mem->client_name[i], "(no name)");
        share_mem->client_pid[i] = -1;
        strcpy(share_mem->ip_port_table[i], "NULL");
        for(int j = 0; j < 30; j++)
        {
            share_mem->usr_pipe_table[i][j] = 0;
            share_mem->usr_pipe_fd[i][j] = 0;
            strcpy(share_mem->tell_table[i][j], "NULL");
        }
    }
}

void tell(vector <string> args)
{
    char tmp[100];
    sprintf(tmp, "*** %s told you ***: %s\n", share_mem->client_name[client_id-1], args[2].c_str());
    //-----LOCK--------------------
    if(share_mem -> client_pid[stoi(args[1])-1] != -1)
    {
        strcpy(share_mem -> tell_table[client_id-1][stoi(args[1])-1], tmp);
        //share_mem -> tell_table[client_id-1][stoi(args[1])-1] = args[2].c_str();
        kill(share_mem -> client_pid[stoi(args[1])-1], SIGUSR1);
    }
    else
    {
        printf("*** Error: user #%s does not exist yet. ***\n", args[1].c_str());
    }
}

void yell(char* msg)
{
    for(int i = 0; i < 30; i++)
    {
        if(share_mem -> client_pid[i] != -1)
        {
            strcpy(share_mem -> tell_table[client_id-1][i], msg);
            kill(share_mem -> client_pid[i], SIGUSR1);
        }
    }
}

void who()
{
    printf("<ID>    <nickname>    <IP:port>   <indicate me>\n");
    for(int i = 0; i < 30; i++)
    {
        if(share_mem ->client_pid[i] != -1)
        {
            if(i+1 == client_id)
            {
                printf("%d    %s    %s    <-me\n", i+1, 
                        share_mem->client_name[i],
                        share_mem->ip_port_table[i]);
            }
            else
            {
                printf("%d    %s    %s    \n", i+1,
                        share_mem->client_name[i],
                        share_mem->ip_port_table[i]);
            }
        }
    }
}

void name(vector <string> args)
{
    strcpy(share_mem->client_name[client_id-1], args[1].c_str());
}

void clear_sharemem_info(void)
{
    // LOCK
    strcpy(share_mem->client_name[client_id-1], "(no name)");
    share_mem -> client_pid[client_id-1] = -1;
    strcpy(share_mem->ip_port_table[client_id-1], "NULL");
    for(int i = 0; i < 30; i++)
    {
        strcpy(share_mem->tell_table[i][client_id-1], "NULL");
        //strcpy(share_mem->tell_table[client_id-1][i], "NULL");
        share_mem -> usr_pipe_table[i][client_id-1] = 0;
        share_mem -> usr_pipe_table[client_id-1][i] = 0;
    }
    share_mem -> lock = 0;
}

int check_builtin(vector <string> args, int socket_fd)
{
    int is_builtin = 1;
    if(args[0] == "printenv"){printenv(args);}
    else if(args[0] == "exit")
    {
        //-----LOCK--------------------
        char tmp[100];
        sprintf(tmp, "*** User '%s' left. ***\n", share_mem->client_name[client_id-1]);
        yell(tmp);
        clear_sharemem_info();
        close(socket_fd);
        exit(0);
    }
    else if(args[0] == "setenv"){setenv(args[1].c_str(), args[2].c_str(), 1);}
    else if(args[0] == "tell"){tell(args);}
    else if(args[0] == "yell")
    {
        char tmp[100];
        sprintf(tmp, "*** %s yelled ***: %s\n", 
                share_mem->client_name[client_id-1], 
                args[1].c_str());
        yell(tmp);
    }
    else if(args[0] == "who"){who();}
    else if(args[0] == "name")
    {
        int name_used = 0;
        for(int name_idx = 0; name_idx < 30; name_idx++)
        {
            if(share_mem->client_pid[name_idx] != -1)
            {
                if(args[1] == share_mem->client_name[name_idx])
                {
                    name_used = 1;
                    break;
                }
            }
        }
        char tmp[100];
        if(name_used)
        {
            printf("*** User '%s' already exists. ***\n", args[1].c_str());
        }
        else
        {
            strcpy(share_mem->client_name[client_id-1], args[1].c_str());
            sprintf(tmp, "*** User from %s is named '%s'. ***\n", 
                    share_mem->ip_port_table[client_id-1], 
                    args[1].c_str());
            yell(tmp);
        }
    }
    else{is_builtin = 0;}
    return is_builtin;
}

vector <command> parse_line(string line)
{
    string temp = "";
    for(int i = 0; i < line.length(); i++)
    {
        fflush(stdout);
        if(line[i] != '\r' && line[i] != '\n'){temp += line[i];}
    }
    line = temp;

    //--Split with space-----------------------------
    string const deli{' '};
    vector <string> tokens;
    size_t beg, pos = 0;
    while ((beg = line.find_first_not_of(deli, pos)) != string::npos)
    {
        pos = line.find_first_of(deli, beg + 1);
        tokens.push_back(line.substr(beg, pos - beg));
    }
    //-----------------------------------------------
    vector <command> cmd_line;
    int line_end = 0;
    int i = 0;
    while(!line_end)
    {
        command cmd;
        while(true)
        {
            for(i; i < tokens.size(); i++)
            {
                if(tokens[i] == "tell" || tokens[i] == "yell")
                {
                    if(tokens[i] == "tell")
                    {
                        cmd.args.push_back(tokens[i]);
                        cmd.args.push_back(tokens[i+1]);
                        for(int tmp_idx = 0; tmp_idx < (int)temp.length(); tmp_idx++)
                        {
                            if(temp.substr(tmp_idx, tmp_idx+4) == "tell")
                            {
                                if(isdigit(temp[tmp_idx+6]))
                                {
                                    cmd.args.push_back(temp.substr(tmp_idx+8, (int)temp.length()));
                                }
                                else
                                {
                                    cmd.args.push_back(temp.substr(tmp_idx+7, (int)temp.length()));
                                }
                                break;
                            }
                        }
                    }
                    else
                    {
                        cmd.args.push_back(tokens[i]);
                        for(int tmp_idx = 0; tmp_idx < (int)temp.length(); tmp_idx++)
                        {
                            if(temp.substr(tmp_idx, tmp_idx+4) == "yell")
                            {
                                cmd.args.push_back(temp.substr(tmp_idx+5, (int)temp.length()));
                                break;
                            }
                        }
                    }
                    
                    i = tokens.size();
                    break;
                }
                if(tokens[i][0] == '|')//pipe or num pipe
                {
                    if(tokens[i][1])// |1
                    {
                        cmd.type = "num_pipe";
                        cmd.num_pipe = stoi(tokens[i].substr(1, string::npos));
                    }
                    else// |
                    {
                        cmd.type = "pipe";
                    }
                    i++;
                    break;
                }
                else if(tokens[i][0] == '>')//file pipe or user pipe
                {
                    if(tokens[i][1])
                    {
                        int is_out_usrpipe = 1;
                        if(i + 1 < tokens.size())
                        {
                            if(tokens[i+1][0] == '<')// >1 <2
                            {
                                is_out_usrpipe = 0;
                                cmd.type = "in_out_user_pipe";
                                cmd.in_usr_id = stoi(tokens[i+1].substr(1, string::npos));
                                cmd.out_usr_id = stoi(tokens[i].substr(1, string::npos));
                                i++;
                            }
                        }
                        if(is_out_usrpipe)// >1
                        {
                            cmd.type = "out_user_pipe";
                            cmd.out_usr_id = stoi(tokens[i].substr(1, string::npos));
                        }
                    }
                    else
                    {
                        int is_in_file_pipe = 0;
                        if(i + 2 < tokens.size())
                        {
                            if(tokens[i+2][0] == '<')// > file <1
                            {
                                is_in_file_pipe = 1;
                                cmd.type = "in_file_user_pipe";
                                cmd.in_usr_id = stoi(tokens[i+2].substr(1, string::npos));
                                cmd.file = tokens[i+1];
                                i++;
                            }
                        }
                        if(!is_in_file_pipe)// > file
                        {
                            cmd.type = "file_pipe";
                            cmd.file = tokens[i+1];
                        }
                        i++;
                    }
                    i++;
                    if(i >= tokens.size()){break;}
                }
                else if(tokens[i][0] == '!')//err pipe or err num pipe
                {
                    if(tokens[i][1])// !1
                    {
                        cmd.type = "err_num_pipe";
                        cmd.num_pipe = stoi(tokens[i].substr(1, string::npos));
                    }
                    else// !
                    {
                        cmd.type = "err_pipe";
                    }
                    i++;
                    break;
                }
                else if(tokens[i][0] == '<')
                {
                    if(tokens[i][1])
                    {
                        int is_in_usrpipe = 1;
                        if(i + 1 < tokens.size())
                        {
                            if(tokens[i+1][0] == '>')
                            {
                                is_in_usrpipe = 0;
                                if(tokens[i+1][1])// <1 >2
                                {
                                    cmd.type = "in_out_user_pipe";
                                    cmd.in_usr_id = stoi(tokens[i].substr(1, string::npos));
                                    cmd.out_usr_id = stoi(tokens[i+1].substr(1, string::npos));
                                }
                                else// <1 > file
                                {
                                    cmd.type = "in_file_user_pipe";
                                    cmd.in_usr_id = stoi(tokens[i].substr(1, string::npos));
                                    cmd.file = tokens[i+2];
                                    i++;
                                }
                                i++;
                                i++;
                                break;
                            }
                            else if(tokens[i+1][0] == '|')
                            {
                                is_in_usrpipe = 0;
                                if(tokens[i+1][1])// <1 |2
                                {
                                    cmd.type = "in_numpipe_user_pipe";
                                    cmd.num_pipe = stoi(tokens[i+1].substr(1, string::npos));
                                    cmd.in_usr_id = stoi(tokens[i].substr(1, string::npos));
                                }
                                else// <1 |
                                {
                                    cmd.type = "in_pipe_user_pipe";
                                    cmd.in_usr_id = stoi(tokens[i].substr(1, string::npos));
                                }
                                i++;
                                i++;
                                break;
                            }
                        }
                        if(is_in_usrpipe)// <1
                        {
                            cmd.type = "in_user_pipe";
                            cmd.in_usr_id = stoi(tokens[i].substr(1, string::npos));
                            i++;
                            break;
                        }
                    }
                }
                else//args
                {
                    cmd.args.push_back(tokens[i]);
                }
            }
            if(i >= tokens.size()){line_end = 1;}
            break;
        } 
        cmd_line.push_back(cmd);
    }
    return cmd_line;
}

int execute_cmd(vector <string> args)//Execute bin command
{
    char * exec_args[1024];
    int arg_count = 0;
    for (int x = 0; x < args.size(); x++) 
    {
        exec_args[arg_count++] = strdup(args[x].c_str());
    }
    exec_args[arg_count++] = 0; // tell it when to stop!
    int status = execvp(exec_args[0], exec_args);
    if(status == -1)
    {
        char tmp[100] = "";
        strcat(tmp, "Unknown command: [");
        strcat(tmp, exec_args[0]);
        strcat(tmp, "].\n");
        write(2, tmp, strlen(tmp));
        exit(0);
    }
    return status;
}

void childHandler(int signo) 
{
    int status;
    while (waitpid(-1, &status, WNOHANG) > 0) 
    {
        //do nothing
    }
}

void tell_handler(int signo)
{
    for(int i = 0; i < 30; i++)
    {
        //if(share_mem -> tell_table[i][client_id-1] != "NULL" )
        if(strcmp(share_mem -> tell_table[i][client_id-1] , "NULL" ) != 0)
        {
            printf("%s", share_mem -> tell_table[i][client_id-1]);
            strcpy(share_mem -> tell_table[i][client_id-1], "NULL");
            //share_mem -> tell_table[i][client_id-1] = "NULL";
        }
    }
    return;
}

void user_pipe_handler(int signo)
{
    for(int i = 0; i < 30; i++)
    {

        if(share_mem -> usr_pipe_table[i][client_id-1] != 0 &&
            share_mem ->usr_pipe_fd[i][client_id-1] == 0)
        {   
            char filename[30];
            sprintf(filename, "user_pipe/%d_%d", i+1, client_id);
            mkfifo(filename, 0666);
            share_mem ->usr_pipe_fd[i][client_id-1] = open(filename, O_RDONLY);
            break;
        }
    }
    
}


void shell_loop(int socket_fd)
{
    dup2(socket_fd, STDOUT_FILENO);
    dup2(socket_fd, STDIN_FILENO);
    dup2(socket_fd, STDERR_FILENO);
    
    string line;
    string const DELI{" "};
    vector <number_pipe> numpipe_table;
    pid_t pid;
    int cmd_no = 1;
    while(true)
    {
        signal(SIGUSR1, tell_handler);
        signal(SIGUSR2, user_pipe_handler);
        printf("%% ");
        getline(cin, line);
        if(line.empty()){continue;}
        vector <command> cmd_pack;
        cmd_pack = parse_line(line);
        for(int i = 0; i < cmd_pack.size(); i++)
        {
            //--Check builtin--------------------
            int is_builtin = 0;
            is_builtin = check_builtin(cmd_pack[i].args, socket_fd);
            if(is_builtin)
            {
                cmd_no++;
                continue;
            }
            //-----------------------------------

            //--Set stdin stdout pipe------------
            int stdin_fd = socket_fd;
            int stdout_fd = socket_fd;
            int is_target = 0;
            int is_usr_target = 0;
            int target_infd[2];

            //----Set stdin to pipe or numpipe--------------
            for(int j = 0; j < numpipe_table.size(); j++)
            {
                if(cmd_no == numpipe_table[j].target_cmd_num)
                {
                    close(numpipe_table[j].out_fd);
                    is_target = 1;
                    stdin_fd = numpipe_table[j].in_fd;
                    break;
                }
            }

            //----Set stdout to pipe------------------------
            if(cmd_pack[i].type == "pipe"||cmd_pack[i].type == "err_pipe" 
                ||cmd_pack[i].type == "in_pipe_user_pipe")
            {
                for(int j = 0; j < numpipe_table.size(); j++)
                {
                    if(cmd_no + 1 == numpipe_table[j].target_cmd_num)
                    {
                        stdout_fd = numpipe_table[j].out_fd;
                        break;
                    }
                }
                if(stdout_fd == socket_fd)
                {
                    int fd[2];
                    pipe(fd);
                    struct number_pipe target;
                    target.in_fd = fd[0];
                    target.out_fd = fd[1];
                    target.target_cmd_num = cmd_no + 1;
                    numpipe_table.push_back(target);
                    stdout_fd = fd[1];
                }
            }
            
            //----Set stdout to numpipe---------------------
            if(cmd_pack[i].type == "num_pipe"||cmd_pack[i].type == "err_num_pipe"
                ||cmd_pack[i].type == "in_numpipe_user_pipe")
            {
                for(int j = 0; j < numpipe_table.size(); j++)
                {
                    if(cmd_no + cmd_pack[i].num_pipe == numpipe_table[j].target_cmd_num)
                    {
                        stdout_fd = numpipe_table[j].out_fd;
                        break;
                    }
                }
                if(stdout_fd == socket_fd)
                {
                    int fd[2];
                    pipe(fd);
                    struct number_pipe target;
                    target.in_fd = fd[0];
                    target.out_fd = fd[1];
                    target.target_cmd_num = cmd_no + cmd_pack[i].num_pipe;
                    numpipe_table.push_back(target);
                    stdout_fd = fd[1];
                }
            }

            //----Set stdin to user pipe--------------------
            if(cmd_pack[i].type == "in_user_pipe"||cmd_pack[i].type == "in_out_user_pipe"
                ||cmd_pack[i].type == "in_file_user_pipe"||cmd_pack[i].type == "in_pipe_user_pipe"
                ||cmd_pack[i].type == "in_numpipe_user_pipe")
            {
                
                //--------Read User Pipe------------------------
                if(share_mem->client_pid[cmd_pack[i].in_usr_id-1] == -1)
                // if pid = -1 -> it doesn't exist.
                {
                    char tmp[100];
                    sprintf(tmp, "*** Error: user #%d does not exist yet. ***\n", cmd_pack[i].in_usr_id);
                    write(socket_fd, tmp, strlen(tmp));
                    int devNull = open("/dev/null", O_WRONLY);
                    stdin_fd = devNull;
                    cmd_no++;
                    continue;
                }
                else if(share_mem->usr_pipe_table[cmd_pack[i].in_usr_id-1][client_id-1] == 0)
                {
                    char tmp[100];
                    sprintf(tmp, "*** Error: the pipe #%d->#%d does not exist yet. ***\n", 
                            cmd_pack[i].in_usr_id, client_id);
                    write(socket_fd, tmp, strlen(tmp));
                    int devNull = open("/dev/null", O_WRONLY);
                    stdin_fd = devNull;
                    cmd_no++;
                    continue;
                }
                else
                {
                    is_usr_target = 1;
                    stdin_fd = share_mem -> usr_pipe_fd[cmd_pack[i].in_usr_id-1][client_id-1];
                    char tmp[100];
                    sprintf(tmp, "*** %s (#%d) just received from %s (#%d) by '%s' ***\n", 
                            share_mem->client_name[client_id-1],
                            client_id, share_mem->client_name[cmd_pack[i].in_usr_id-1],
                            cmd_pack[i].in_usr_id, line.c_str());
                    yell(tmp);
                }
                //-----------------------------------------------
            }
            //----Set stdout to user pipe------------------
            if(cmd_pack[i].type == "out_user_pipe"||cmd_pack[i].type =="in_out_user_pipe")
            {
                //---------Write User Pipe-----------------------
                if(share_mem->client_pid[cmd_pack[i].out_usr_id-1] == -1)
                // if pid = -1 -> it doesn't exist.
                {
                    char tmp[100];
                    sprintf(tmp, "*** Error: user #%d does not exist yet. ***\n", cmd_pack[i].out_usr_id);
                    write(socket_fd, tmp, strlen(tmp));
                    cmd_no++;
                    continue;
                }
                else if(share_mem->usr_pipe_table[client_id-1][cmd_pack[i].out_usr_id-1] != 0)
                {
                    char tmp[100];
                    sprintf(tmp, "*** Error: the pipe #%d->#%d already exists. ***\n", 
                            client_id, cmd_pack[i].out_usr_id);
                    write(socket_fd, tmp, strlen(tmp));
                    cmd_no++;
                    continue;
                }
                else
                {
                    //signal target pid and open FIFO
                    share_mem ->usr_pipe_table[client_id-1][cmd_pack[i].out_usr_id-1] = 1;
                    char filename[30];
                    sprintf(filename, "user_pipe/%d_%d", client_id, cmd_pack[i].out_usr_id);
                    mkfifo(filename, 0666);
                    kill(share_mem->client_pid[cmd_pack[i].out_usr_id-1], SIGUSR2);
                    stdout_fd = open(filename, O_WRONLY);
                    unlink(filename);
                    char tmp[100];
                    sprintf(tmp, "*** %s (#%d) just piped '%s' to %s (#%d) ***\n", 
                            share_mem->client_name[client_id-1],
                            client_id, line.c_str(), share_mem->client_name[cmd_pack[i].out_usr_id-1],
                            cmd_pack[i].out_usr_id);
                    yell(tmp);
                }
                //-----------------------------------------------
            }

            //----Set stdout to file pipe------------------
            if(cmd_pack[i].type == "file_pipe"||cmd_pack[i].type == "in_file_user_pipe")
            {
                stdout_fd = open(cmd_pack[i].file.c_str(), O_RDWR|O_CREAT|O_TRUNC, 0666);
            }
            //-----------------------------------
            signal(SIGCHLD, childHandler);
            pid_t pid = fork();
            if(pid == 0)
            {
                if(cmd_pack[i].type == "err_num_pipe" or cmd_pack[i].type == "err_pipe")
                {
                    dup2(stdout_fd, STDERR_FILENO);
                }
                dup2(stdin_fd, STDIN_FILENO);
                dup2(stdout_fd, STDOUT_FILENO);
                execute_cmd(cmd_pack[i].args);
                exit(0);
            }
            else
            {
                int lineEndsWithPipeN = 0;
                if(cmd_pack[cmd_pack.size()-1].type == "num_pipe" 
                    or cmd_pack[cmd_pack.size()-1].type == "err_num_pipe")
                {
                    lineEndsWithPipeN = 1;
                }

                if(is_target || is_usr_target)
                {
                    close(stdin_fd);
                }
                
                if(cmd_pack[i].type == "in_user_pipe"||cmd_pack[i].type == "in_out_user_pipe"
                    ||cmd_pack[i].type == "in_file_user_pipe"||cmd_pack[i].type == "in_pipe_user_pipe"
                    ||cmd_pack[i].type == "in_numpipe_user_pipe")
                {
                    share_mem -> usr_pipe_table[cmd_pack[i].in_usr_id-1][client_id-1] = 0;
                    share_mem -> usr_pipe_fd[cmd_pack[i].in_usr_id-1][client_id-1] = 0;
                }
                
                if(cmd_pack[i].type == "out_user_pipe"||cmd_pack[i].type =="in_out_user_pipe")
                {
                    close(stdout_fd);
                }

                if(!lineEndsWithPipeN && cmd_pack[i].type != "out_user_pipe" 
                    && cmd_pack[i].type != "in_out_user_pipe" && (i == cmd_pack.size()-1))
                {
                    int status;
                    waitpid(pid, &status, 0);
                }
            }
            cmd_no++;
        }
    }
}


int main(int argc, char *argv[])
{
    init_share_mem();
    clearenv();
    setenv("PATH", "bin:.", 1);
    int server_fd, new_socket, valread; 
    struct sockaddr_in address; 
    int opt = 1; 
    int addrlen = sizeof(address); 
    char buffer[1024] = {0}; 
    
    // Creating socket file descriptor 
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) 
    { 
        perror("socket failed"); 
        exit(EXIT_FAILURE); 
    } 
    
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) 
    { 
        perror("setsockopt"); 
        exit(EXIT_FAILURE); 
    }
    if(argc < 2)
    {
        perror("In address not found");
        exit(EXIT_FAILURE);
    }
    int PORT = atoi(argv[1]);
    address.sin_family = AF_INET; 
    address.sin_addr.s_addr = INADDR_ANY; 
    address.sin_port = htons( PORT ); 
    
    if (bind(server_fd, (struct sockaddr *)&address,  
                                 sizeof(address))<0) 
    { 
        perror("bind failed"); 
        exit(EXIT_FAILURE); 
    } 
    
    while(1)
    {
        if (listen(server_fd, 3) < 0) 
        { 
            perror("listen"); 
            exit(EXIT_FAILURE); 
        } 

        if ((new_socket = accept(server_fd, (struct sockaddr *)&address,  
                       (socklen_t*)&addrlen))<0) 
        { 
            perror("accept"); 
            exit(EXIT_FAILURE); 
        }
        else
        {
            //------Send wellcome message------------
            string wellcome_msg = message;
            wellcome_msg += string(inet_ntoa(address.sin_addr));
            wellcome_msg += ":";
            wellcome_msg += to_string(ntohs(address.sin_port));
            wellcome_msg += ". ***\n";
            if(send(new_socket, wellcome_msg.c_str(), strlen(wellcome_msg.c_str()), 0) 
                != strlen(wellcome_msg.c_str()))   
            {   
                perror("send");   
            }
            puts("Welcome message sent successfully"); 
            //---------------------------------------
            string ip_port = string(inet_ntoa(address.sin_addr));
            ip_port += ":";
            ip_port += to_string(ntohs(address.sin_port));
            char msg[100];
            sprintf(msg, "*** User '(no name)' entered from %s. ***\n", ip_port.c_str());
            yell(msg);
            int shm_idx = -1;
            for(int i = 0; i < 30; i++)
            {
                if(share_mem -> client_pid[i] == -1)
                {
                    
                    strcpy(share_mem -> ip_port_table[i], ip_port.c_str());
                    //share_mem -> ip_port_table[i] = ip_port;
                    shm_idx = i;
                    client_id = i+1;
                    break;
                }
            }
            
            pid_t pid = fork();
            if(pid == 0)
            {
                shell_loop(new_socket);
                break;
            }
            else
            {
                //-----LOCK--------------------
                share_mem -> client_pid[shm_idx] = pid;
                close(new_socket);
            }
        }
    }
    return 0;
}