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
using namespace std;

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
    else if(args[0] == "exit"){exit(0);}
    else if(args[0] == "setenv"){setenv(args[1].c_str(), args[2].c_str(), 1);}
    else{is_builtin = 0;}
    return is_builtin;
}
//----------------------------------------------------

vector <string> parse_line(string line, string const delimiter)
{
    vector <string> tokens;
    size_t beg, pos = 0;
    while ((beg = line.find_first_not_of(delimiter, pos)) != string::npos)
    {
        pos = line.find_first_of(delimiter, beg + 1);
        tokens.push_back(line.substr(beg, pos - beg));
    }
    return tokens;
}

struct command{
    vector<string> args;
    string type;
    string file;
    int num_pipe;
};

struct number_pipe
{
    int target_cmd_num;
    int in_fd;
    int out_fd;
};


vector <command> parse_line_test(string line)
{
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
                if(tokens[i][0] == '|')//pipe or num pipe
                {
                    if(tokens[i][1])
                    {
                        cmd.type = "num_pipe";
                        cmd.num_pipe = stoi(tokens[i].substr(1, string::npos));
                    }
                    else{cmd.type = "pipe";}
                    i++;
                    break;
                }
                else if(tokens[i][0] == '>')//file pipe or user pipe
                {
                    if(tokens[i][1])
                    {
                        cmd.type = "user_pipe";
                    }
                    else
                    {
                        cmd.type = "file_pipe";
                        cmd.file = tokens[i+1];
                        i++;
                    }
                    i++;
                }
                else if(tokens[i][0] == '!')//err pipe or err num pipe
                {
                    if(tokens[i][1])
                    {
                        cmd.type = "err_num_pipe";
                        cmd.num_pipe = stoi(tokens[i].substr(1, string::npos));
                    }
                    else{cmd.type = "err_pipe";}
                    i++;
                    break;
                }
                else if(tokens[i][0] == '<')//user pipe
                {
                    if(tokens[i][1])
                    {
                        cmd.type = "user_pipe";
                    }
                }
                else//args
                {
                    cmd.args.push_back(tokens[i]);
                }
            }
            if(i == tokens.size()){line_end = 1;}
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
    for (int x = 0; x < args.size(); x++) {
    exec_args[arg_count++] = strdup(args[x].c_str());
    }
    exec_args[arg_count++] = 0; // tell it when to stop!
    int status = execvp(exec_args[0], exec_args);
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


void shell_loop()
{
    string line;
    string const DELI{" "};
    vector <number_pipe> numpipe_table;
    pid_t pid;
    int cmd_no = 1;
    while(true)
    {
        printf("> ");
        getline(cin, line);
        vector <command> cmd_pack;
        cmd_pack = parse_line_test(line);
        // for(int i = 0; i < cmd_pack.size(); i++)
        // {
        //     printf("cmd: ");
        //     for(int j = 0; j < cmd_pack[i].args.size(); j++)
        //     {
        //         printf("%s ", cmd_pack[i].args[j].c_str());
        //     }
        //     printf(", cmd_type: %s \n", cmd_pack[i].type.c_str());
        // }
        for(int i = 0; i < cmd_pack.size(); i++)
        {
            int stdin_fd = STDIN_FILENO;
            int stdout_fd = STDOUT_FILENO;
            int is_target = 0;
            //--Check if current cmd is other's target---------
            int stdin_pipe[2];
            for(int k = 0; k < numpipe_table.size(); k++)
            {
                if(cmd_no == numpipe_table[k].target_cmd_num)
                {
                    is_target = 1;
                    stdin_fd = numpipe_table[k].in_fd;
                    stdin_pipe[0] = numpipe_table[k].in_fd;
                    stdin_pipe[1] = numpipe_table[k].out_fd;
                    close(numpipe_table[k].out_fd);
                }
            }
            //-------------------------------------------------

            if(cmd_pack[i].type == "pipe")
            {
                for(int j = 0; j < numpipe_table.size(); j++)
                {
                    if(cmd_no + 1 == numpipe_table[j].target_cmd_num)
                    {
                        stdout_fd = numpipe_table[j].out_fd;
                    }
                }
                if(stdout_fd == STDOUT_FILENO)
                {
                    int fd[2];
                    pipe(fd);
                    stdout_fd = fd[1];
                    //--Update numpipe table-------------
                    number_pipe target;
                    target.in_fd = fd[0];
                    target.out_fd = fd[1];
                    target.target_cmd_num = cmd_no + 1;
                    numpipe_table.push_back(target); 
                    //-----------------------------------
                }
            }
            else if(cmd_pack[i].type == "num_pipe" or cmd_pack[i].type == "err_num_pipe")
            {
                for(int j = 0; j < numpipe_table.size(); j++)
                {
                    if(cmd_no + cmd_pack[i].num_pipe == numpipe_table[j].target_cmd_num)
                    {
                        stdout_fd = numpipe_table[j].out_fd;
                    }
                }
                if(stdout_fd == STDOUT_FILENO)
                {
                    int fd[2];
                    pipe(fd);
                    stdout_fd = fd[1];
                    //--Update numpipe table-------------
                    number_pipe target;
                    target.in_fd = fd[0];
                    target.out_fd = fd[1];
                    target.target_cmd_num = cmd_no + cmd_pack[i].num_pipe;
                    numpipe_table.push_back(target); 
                    //-----------------------------------
                }
            }
            signal(SIGCHLD, childHandler);
            pid = fork();
            while(pid < 0)
            {
                pid = fork();
                usleep(1000);
            }
            if(pid == 0)
            {
                dup2(stdin_fd, STDIN_FILENO);
                if(cmd_pack[i].type == "err_pipe" or cmd_pack[i].type == "err_num_pipe")
                {
                    dup2(stdout_fd, STDERR_FILENO);
                }
                else if(cmd_pack[i].type == "file_pipe")
                {
                    stdout_fd = open(cmd_pack[i].file.c_str(), O_RDWR|O_CREAT|O_TRUNC, 0666);
                }
                dup2(stdout_fd, STDOUT_FILENO);
                int status = execute_cmd(cmd_pack[i].args);
                printf("%d\n", status);
            }
            else
            {
                if(is_target){close(stdin_pipe[0]);}
                int lineEndsWithPipeN = 0;
                if(cmd_pack[cmd_pack.size()].type == "num_pipe")
                {
                    lineEndsWithPipeN = 1;
                }
                if(!lineEndsWithPipeN && i == cmd_pack.size())
                {
                    int status_child;
                    waitpid(pid, &status_child, 0);
                }
            }
            cmd_no++;
        }
    }
}


int main()
{
    setenv("PATH", "bin:.", 1);
    shell_loop();
    return 0;
}