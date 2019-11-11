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
int exit_flag = 0;
string message = 
"***************************************\n\
** Welcome to the information server **\n\
***************************************\n\
*** User ’(no name)’ entered from ";


//-----Built in command-------------------------------
void printenv(vector <string> args)
{
    string env = getenv(args[1].c_str());
    if(env!=""){printf("%s\n", env.c_str());}
}

int check_builtin(vector <string> args)
{
    int is_builtin = 1;
    if(args[0] == "printenv"){printenv(args);}
    else if(args[0] == "exit")
    {
        exit_flag = 1;
    }
    else if(args[0] == "setenv"){setenv(args[1].c_str(), args[2].c_str(), 1);}
    else{is_builtin = 0;}
    return is_builtin;
}
//----------------------------------------------------

struct client_share_memory
{
    string client_name;
    int client_pid;
    int usr_pipe_table[30];
    string tell_table[30];
};


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
                                cmd.args.push_back(temp.substr(tmp_idx+6, (int)temp.length()));
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
        write(2, tmp, sizeof(char)*100);
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
        printf("%% ");
        getline(cin, line);
        if(line.empty()){continue;}
        vector <command> cmd_pack;
        cmd_pack = parse_line(line);
        for(int i = 0; i < cmd_pack.size(); i++)
        {
            //--Check builtin--------------------
            int is_builtin = 0;
            is_builtin = check_builtin(cmd_pack[i].args);
            printf("EXIT\n");
            if(is_builtin)
            {
                if(exit_flag)
                {
                    break;
                }
                cmd_no++;
                continue;
            }
            //-----------------------------------

            //--Set stdin stdout pipe------------
            int stdin_fd = socket_fd;
            int stdout_fd = socket_fd;
            int is_target = 0;
            int target_infd[2];

            for(int j = 0; j < numpipe_table.size(); j++)
            {
                if(cmd_no == numpipe_table[j].target_cmd_num)
                {
                    close(numpipe_table[j].out_fd);
                    is_target = 1;
                    stdin_fd = numpipe_table[j].in_fd;
                    target_infd[0] = numpipe_table[j].in_fd;
                    target_infd[1] = numpipe_table[j].out_fd;
                    break;
                }
            }
            if(cmd_pack[i].type == "pipe" or cmd_pack[i].type == "err_pipe")
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
            else if(cmd_pack[i].type == "num_pipe" or cmd_pack[i].type == "err_num_pipe")
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
            else if(cmd_pack[i].type == "file_pipe")
            {
                stdout_fd = open(cmd_pack[i].file.c_str(), O_RDWR|O_CREAT|O_TRUNC, 0666);
            }
            else if(cmd_pack[i].type == "in_out_user_pipe")
            {

            }
            else if(cmd_pack[i].type == "in_file_user_pipe")
            {
                
            }
            else if(cmd_pack[i].type == "out_user_pipe")
            {
                
            }
            else if(cmd_pack[i].type == "in_user_pipe")
            {
                
            }
            else if(cmd_pack[i].type == "in_numpipe_user_pipe")
            {
                
            }
            else if(cmd_pack[i].type == "in_pipe_user_pipe")
            {
                
            }
            //-----------------------------------

            pid_t pid = fork();
            if(pid == 0)
            {
                if(cmd_pack[i].type == "err_num_pipe" or cmd_pack[i].type == "err_pipe")
                {
                    dup2(stdout_fd, STDERR_FILENO);
                }
                dup2(stdin_fd, STDIN_FILENO);
                dup2(stdout_fd, STDOUT_FILENO);
                for(int o = 3; o < 1024; o++){close(o);}
                execute_cmd(cmd_pack[i].args);
                exit(0);
            }
            else
            {
                int lineEndsWithPipeN = 0;
                if(cmd_pack[cmd_pack.size()-1].type == "num_pipe" or cmd_pack[cmd_pack.size()-1].type == "err_num_pipe")
                {
                    lineEndsWithPipeN = 1;
                }
                if(is_target){close(target_infd[0]);}
                if(!lineEndsWithPipeN && (i == cmd_pack.size()-1))
                {
                    int status;
                    waitpid(pid, &status, 0);
                }
            }
            cmd_no++;
        }
        
        if(exit_flag)
        {
            printf("EXIT\n");
            exit_flag = 0;
            //close(socket_fd);
            break;
        }
    }
    printf("EXIT\n");
}


int main(int argc, char *argv[])
{
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

            pid_t pid = fork();
            if(pid == 0)
            {
                shell_loop(new_socket);
                printf("EXIT\n");
                break;
            }
            else
            {
                close(new_socket);
            }
        }
    }
    return 0;
}