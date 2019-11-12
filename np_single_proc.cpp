#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <cstring>
#include <fstream>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

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

struct connection_info
{
    int socket_fd;
    int cmd_no;
    int user_id;
    string user_name;
    string ip_port;
    vector <number_pipe> numpipe_table;
    map <string, string> env;
};

struct user_pipe
{
    int in_fd;
    int out_fd;
    user_pipe()
    {
        in_fd = -1;
        out_fd = -1;
    }
};

//-----Built in command-------------------------------
void printenv(vector <string> args, int socket_fd)
{
    string env = getenv(args[1].c_str());
    if(env!="")
    {
        char tmp[200];
        sprintf(tmp, "%s\n", env.c_str());
        write(socket_fd, tmp, strlen(tmp));
    }
}

void who(vector <connection_info> connect_info_table, int socket_fd)
{
    char tmp[200];
    sprintf(tmp, "<ID>    <nickname>    <IP:port>   <indicate me>\n");
    write(socket_fd, tmp, strlen(tmp));
    for(int j = 1; j <= 30; j++)
    {
        for(int i = 0; i < connect_info_table.size(); i++)
        {
            if(j == connect_info_table[i].user_id)
            {
                if(socket_fd == connect_info_table[i].socket_fd)
                {
                    sprintf(tmp, "%d    %s    %s    <-me\n", j, 
                            connect_info_table[i].user_name.c_str(),
                            connect_info_table[i].ip_port.c_str());
                    write(socket_fd, tmp, strlen(tmp));
                }
                else
                {
                    sprintf(tmp, "%d    %s    %s    \n", j, 
                            connect_info_table[i].user_name.c_str(), 
                            connect_info_table[i].ip_port.c_str());
                    write(socket_fd, tmp, strlen(tmp));
                }
            }
        }
    }
}

void name(string name, vector <connection_info> &connect_info_table, int socket_fd)
{
    int name_been_used = 0;
    for(int tmp_idx = 0; tmp_idx < connect_info_table.size(); tmp_idx++)
    {
        if(name == connect_info_table[tmp_idx].user_name)
        {
            name_been_used = 1;
            char tmp[50];
            sprintf(tmp, "*** User '%s' already exists. ***\n", name.c_str());
            write(socket_fd, tmp, strlen(tmp));
            break;
        }
    }
    if(!name_been_used)
    {
        string message;
        for(int i = 0; i < connect_info_table.size(); i++)
        {
            if(socket_fd == connect_info_table[i].socket_fd)
            {
                message = "*** User from " + connect_info_table[i].ip_port + " is named '" + name +"'. ***\n";
                connect_info_table[i].user_name = name;
            }
        }

        for(int i = 0; i < connect_info_table.size(); i++)
        {
            send(connect_info_table[i].socket_fd, message.c_str(), message.size(), 0);
        }
    }
}

void tell(vector <string> args, vector <connection_info> connect_info_table, int socket_fd)
{
    string message = "";
    int target_fd = -1;
    for(int i = 0; i < connect_info_table.size(); i++)
    {
        if(socket_fd == connect_info_table[i].socket_fd)
        {
            message = "*** "+connect_info_table[i].user_name+" told you ***: ";
            for(int j = 2; j < args.size(); j++)
            {
                message += args[j];
                if(j != args.size()-1)
                {
                    message += " ";
                }
            }
            message += "\n";
        }
        if(stoi(args[1]) == connect_info_table[i].user_id)
        {
            target_fd = connect_info_table[i].socket_fd;
        }
    }
    if(target_fd == -1)
    {
        char tmp[200];
        sprintf(tmp, "*** Error: user #%s does not exist yet. ***\n", args[1].c_str());
        write(socket_fd, tmp, strlen(tmp));
    }
    else
    {
        send(target_fd, message.c_str(), message.size(), 0);
    }
    
}

void yell(vector <string> args, vector <connection_info> connect_info_table, int socket_fd)
{
    string message = "";
    for(int i = 0; i < connect_info_table.size(); i++)
    {
        if(socket_fd == connect_info_table[i].socket_fd)
        {
            message = "*** "+connect_info_table[i].user_name+" yelled ***: ";
            for(int j = 1; j < args.size(); j++)
            {
                message += args[j];
                if(j != args.size()-1)
                {
                    message += " ";
                }
            }
            message += "\n";
        }
    }
    for(int i = 0; i < connect_info_table.size(); i++)
    {
        send(connect_info_table[i].socket_fd, message.c_str(), message.size(), 0);
    }
}

int check_builtin(vector <string> args, vector <connection_info> &connect_info_table, int socket_fd,
                    vector <int> &client_socket, int client_socket_idx, vector <vector <user_pipe> > &usr_pipe_table)
{
    int connect_info_idx = -1;
    for(int i = 0; i < connect_info_table.size(); i++)
    {
        if(connect_info_table[i].socket_fd == socket_fd)
        {
            connect_info_idx = i;
            break;
        }
    }
    int is_builtin = 1;
    if(args[0] == "printenv"){printenv(args, socket_fd);}
    else if(args[0] == "exit")
    {
        client_socket[client_socket_idx] = 0;
        for(int info_idx = 0; info_idx < connect_info_table.size(); info_idx++)
        {
            char tmp[50];
            sprintf(tmp, "*** User '%s' left. ***\n", connect_info_table[connect_info_idx].user_name.c_str());
            //if(connect_info_table[info_idx].socket_fd != socket_fd)
            
            write(connect_info_table[info_idx].socket_fd, tmp, strlen(tmp));
            
        }
        for(int tmp_idx = 0; tmp_idx < 30 ; tmp_idx ++)
        {
            usr_pipe_table[tmp_idx][connect_info_idx].in_fd = -1;
            usr_pipe_table[tmp_idx][connect_info_idx].out_fd = -1;
        }
        connect_info_table.erase(connect_info_table.begin()+connect_info_idx);
        close(socket_fd);
    }
    else if(args[0] == "setenv")
    {
        setenv(args[1].c_str(), args[2].c_str(), 1);
        connect_info_table[connect_info_idx].env[args[1]] = args[2]; 
    }
    else if(args[0] == "who"){who(connect_info_table, socket_fd);}
    else if(args[0] == "name"){name(args[1], connect_info_table, socket_fd);}
    else if(args[0] == "tell"){tell(args, connect_info_table, socket_fd);}
    else if(args[0] == "yell"){yell(args, connect_info_table, socket_fd);}
    else
    {
        is_builtin = 0;
    }
    return is_builtin;
}
//----------------------------------------------------



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

int execute_cmd(vector <string> args, int socket_fd)//Execute bin command
{
    char * exec_args[1024];
    int arg_count = 0;
    for (int x = 0; x < args.size(); x++) 
    {
        exec_args[arg_count++] = strdup(args[x].c_str());
    }
    exec_args[arg_count++] = 0; // tell it when to stop!
    //printf("DBG msg3\n");
    int status = execvp(exec_args[0], exec_args);
    //printf("DBG msg4\n");
    if(status == -1)
    {
        //printf("DBG msg1\n");
        char tmp[100] = {0};
        strcat(tmp, "Unknown command: [");
        strcat(tmp, exec_args[0]);
        strcat(tmp, "].\n");
        write(socket_fd, tmp, strlen(tmp));
        //printf("DBG msg2\n");
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

int exe_shell_cmd(int socket_fd, int &cmd_no, vector <number_pipe> &numpipe_table,
                    string &line, vector <connection_info> &connect_info_table,
                    vector <vector <user_pipe> > &usr_pipe_table, 
                    vector <int> &client_socket, int client_socket_idx)
{
    // int saved_stdout = dup(STDOUT_FILENO);
    int connect_info_idx = -1;
    int current_usr_id = -1;
    for(int i = 0; i < connect_info_table.size(); i++)
    {
        if(connect_info_table[i].socket_fd == socket_fd)
        {
            current_usr_id = connect_info_table[i].user_id;
            connect_info_idx = i;
            break;
        }
    }

    clearenv();

    for(map<string,string>::iterator iter=connect_info_table[connect_info_idx].env.begin();
        iter!=connect_info_table[connect_info_idx].env.end();iter++)
    {
        setenv(iter->first.c_str(), iter->second.c_str(), 1);
    }

    // string line;
    string const DELI{" "};
    pid_t pid;
    
    if(line=="\r\n")
    {
        return 0;
    }
    string temp = "";
    for(int i = 0; i < line.length(); i++)
    {
        fflush(stdout);
        if(line[i] != '\r' && line[i] != '\n'){temp += line[i];}
    }
    vector <command> cmd_pack;
    cmd_pack = parse_line(line);

    //--Execute cmd one by one---------------
    for(int i = 0; i < cmd_pack.size(); i++)
    {
        printf("===============================\n");
        printf("CMD: %s\n", cmd_pack[i].args[0].c_str());
        for(int tmp_idx = 0; tmp_idx < cmd_pack[i].args.size();tmp_idx ++)
        {
            printf("ARGS: %s\n", cmd_pack[i].args[tmp_idx].c_str());
        }
        printf("===============================\n");


        //--Check builtin--------------------
        int is_builtin = 0;
        is_builtin = check_builtin(cmd_pack[i].args, connect_info_table, 
                                    socket_fd, client_socket, client_socket_idx,
                                    usr_pipe_table);
        if(is_builtin)
        {
            cmd_no++;
            continue;
        }
        //-----------------------------------

        //--Set stdin stdout pipe------------
        int stdin_fd = STDIN_FILENO;
        int stdout_fd = socket_fd;
        int is_target = 0;
        int is_usr_target = 0;
        int target_infd[2];

        //--If current cmd is other's target, change stdin----
        for(int j = 0; j < numpipe_table.size(); j++)
        {
            if(cmd_no == numpipe_table[j].target_cmd_num)
            {
                close(numpipe_table[j].out_fd);
                is_target = 1;
                stdin_fd = numpipe_table[j].in_fd;
                target_infd[0] = numpipe_table[j].in_fd;
                target_infd[1] = numpipe_table[j].out_fd;
                numpipe_table.erase(numpipe_table.begin()+j);
                break;
            }
        }
        //----------------------------------------------------

        //--Record pipe or numpipe----------------------------
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
            int err_flag = 0;

            if(usr_pipe_table[(cmd_pack[i].in_usr_id)-1][current_usr_id-1].in_fd == -1)
            {
                //---write error-------------------
                char tmp[100];
                sprintf(tmp, "*** Error: the pipe #%d->#%d does not exists yet. ***\n", 
                        cmd_pack[i].in_usr_id,current_usr_id);
                write(socket_fd, tmp, strlen(tmp));
                int devNull = open("/dev/null", O_WRONLY);
                stdin_fd = devNull;
                err_flag = 1;
            }
            else
            {
                is_usr_target = 1;
                stdin_fd = usr_pipe_table[(cmd_pack[i].in_usr_id)-1][current_usr_id-1].in_fd;// target to current (in_fd = read end)
            
                close(usr_pipe_table[(cmd_pack[i].in_usr_id)-1][current_usr_id-1].out_fd);
                usr_pipe_table[(cmd_pack[i].in_usr_id)-1][current_usr_id-1].in_fd = -1;
                usr_pipe_table[(cmd_pack[i].in_usr_id)-1][current_usr_id-1].out_fd = -1;
            }

            if(usr_pipe_table[current_usr_id-1][(cmd_pack[i].out_usr_id)-1].in_fd == -1)
            {
                int fd[2];
                pipe(fd);// open a new pipe for target user
                usr_pipe_table[current_usr_id-1][(cmd_pack[i].out_usr_id)-1].in_fd = fd[0];//update usr pipe table
                usr_pipe_table[current_usr_id-1][(cmd_pack[i].out_usr_id)-1].out_fd = fd[1];
                stdout_fd = usr_pipe_table[current_usr_id-1][(cmd_pack[i].out_usr_id)-1].out_fd;
            }
            else
            {
                //---write error-------------------
                int out_usr_exist = 0;
                for(int tmp_idx = 0; tmp_idx < connect_info_table.size();tmp_idx++)
                {
                    if(cmd_pack[i].out_usr_id == connect_info_table[tmp_idx].user_id)
                    {
                        out_usr_exist = 1;
                        break;
                    }
                }

                char tmp[100];
                if(out_usr_exist)
                {
                    sprintf(tmp, "*** Error: the pipe #%d->#%d already exists. ***\n", 
                        current_usr_id,cmd_pack[i].out_usr_id);
                }
                else
                {
                    sprintf(tmp, "*** Error: user #%d does not exist yet. ***\n", cmd_pack[i].out_usr_id);
                }
                write(socket_fd, tmp, strlen(tmp));
                err_flag = 1;
            }


            if(err_flag)
            {
                cmd_no++;
                continue;
            }
            else
            {
                string sender_name = "";
                for(int tmp_idx = 0;tmp_idx < connect_info_table.size();tmp_idx++)
                {
                    if(connect_info_table[tmp_idx].user_id == cmd_pack[i].in_usr_id)
                    {
                        sender_name = connect_info_table[tmp_idx].user_name;
                    }
                }
                for(int info_idx = 0; info_idx < connect_info_table.size(); info_idx++)
                {
                    char tmp[50];
                    sprintf(tmp, "*** %s (#%d) just received from %s (#%d) ***\n", 
                            connect_info_table[connect_info_idx].user_name.c_str(),
                            current_usr_id, sender_name.c_str(), cmd_pack[i].in_usr_id);
                    //if(connect_info_table[info_idx].socket_fd != socket_fd)
                    
                    write(connect_info_table[info_idx].socket_fd, tmp, strlen(tmp)); 
                }


                string receiver_name = "";
                for(int tmp_idx = 0;tmp_idx < connect_info_table.size();tmp_idx++)
                {
                    if(connect_info_table[tmp_idx].user_id == cmd_pack[i].out_usr_id)
                    {
                        receiver_name = connect_info_table[tmp_idx].user_name;
                    }
                }
                for(int info_idx = 0; info_idx < connect_info_table.size(); info_idx++)
                {
                    string cmd = "";
                    for(int tmp_idx = 0; tmp_idx < cmd_pack[i].args.size(); tmp_idx++)
                    {
                        cmd += cmd_pack[i].args[tmp_idx];
                        cmd += " ";
                    }
                    cmd += ">";
                    cmd += to_string(cmd_pack[i].out_usr_id);
                    char tmp[100];
                    sprintf(tmp, "*** %s (#%d) just piped '%s' to %s (#%d) ***\n", 
                            connect_info_table[connect_info_idx].user_name.c_str(),
                            current_usr_id, temp.c_str(), receiver_name.c_str(),
                            cmd_pack[i].out_usr_id);
                    //if(connect_info_table[info_idx].socket_fd != socket_fd)
                    
                    write(connect_info_table[info_idx].socket_fd, tmp, strlen(tmp)); 
                }
            }
            
        }
        else if(cmd_pack[i].type == "in_file_user_pipe")
        {
            printf("Parse correct\n");
            if(usr_pipe_table[(cmd_pack[i].in_usr_id)-1][current_usr_id-1].in_fd == -1)
            {
                //--write error message------------
                int in_usr_exist = 0;
                for(int tmp_idx = 0; tmp_idx < connect_info_table.size();tmp_idx++)
                {
                    if(cmd_pack[i].in_usr_id == connect_info_table[tmp_idx].user_id)
                    {
                        in_usr_exist = 1;
                        break;
                    }
                }

                char tmp[100];
                if(in_usr_exist)
                {
                    sprintf(tmp, "*** Error: the pipe #%d->#%d does not exists yet. ***\n", 
                            cmd_pack[i].in_usr_id,current_usr_id);
                }
                else
                {
                    sprintf(tmp, "*** Error: user #%d does not exist yet. ***\n", cmd_pack[i].in_usr_id);
                }
                write(socket_fd, tmp, strlen(tmp));
                int devNull = open("/dev/null", O_WRONLY);
                stdin_fd = devNull;
                cmd_no++;
                continue;
            }
            else
            {
                is_usr_target = 1;
                stdin_fd = usr_pipe_table[(cmd_pack[i].in_usr_id)-1][current_usr_id-1].in_fd;// target to current (in_fd = read end)
                close(usr_pipe_table[(cmd_pack[i].in_usr_id)-1][current_usr_id-1].out_fd);
                usr_pipe_table[(cmd_pack[i].in_usr_id)-1][current_usr_id-1].in_fd = -1;
                usr_pipe_table[(cmd_pack[i].in_usr_id)-1][current_usr_id-1].out_fd = -1;
                stdout_fd = open(cmd_pack[i].file.c_str(), O_RDWR|O_CREAT|O_TRUNC, 0666);
                
                string sender_name = "";
                for(int tmp_idx = 0;tmp_idx < connect_info_table.size();tmp_idx++)
                {
                    if(connect_info_table[tmp_idx].user_id == cmd_pack[i].in_usr_id)
                    {
                        sender_name = connect_info_table[tmp_idx].user_name;
                    }
                }
                for(int info_idx = 0; info_idx < connect_info_table.size(); info_idx++)
                {
                    char tmp[50];
                    sprintf(tmp, "*** %s (#%d) just received from %s (#%d) ***\n", 
                            connect_info_table[connect_info_idx].user_name.c_str(),
                            current_usr_id, sender_name.c_str(), cmd_pack[i].in_usr_id);
                    //if(connect_info_table[info_idx].socket_fd != socket_fd)
                    
                    write(connect_info_table[info_idx].socket_fd, tmp, strlen(tmp)); 
                }
            }
        }
        else if(cmd_pack[i].type == "out_user_pipe")
        {
            if(usr_pipe_table[current_usr_id-1][(cmd_pack[i].out_usr_id)-1].in_fd == -1)
            {
                int out_usr_exist = 0;
                for(int tmp_idx = 0; tmp_idx < connect_info_table.size();tmp_idx++)
                {
                    if(cmd_pack[i].out_usr_id == connect_info_table[tmp_idx].user_id)
                    {
                        out_usr_exist = 1;
                        break;
                    }
                }

                char tmp[100];
                if(!out_usr_exist)
                {
                    sprintf(tmp, "*** Error: user #%d does not exist yet. ***\n", cmd_pack[i].out_usr_id);
                    cmd_no++;
                    continue;
                }
                else
                {
                    int fd[2];
                    pipe(fd);// open a new pipe for target user
                    usr_pipe_table[current_usr_id-1][(cmd_pack[i].out_usr_id)-1].in_fd = fd[0];//update usr pipe table
                    usr_pipe_table[current_usr_id-1][(cmd_pack[i].out_usr_id)-1].out_fd = fd[1];
                    stdout_fd = usr_pipe_table[current_usr_id-1][(cmd_pack[i].out_usr_id)-1].out_fd;
                    string receiver_name = "";
                    for(int tmp_idx = 0;tmp_idx < connect_info_table.size();tmp_idx++)
                    {
                        if(connect_info_table[tmp_idx].user_id == cmd_pack[i].out_usr_id)
                        {
                            receiver_name = connect_info_table[tmp_idx].user_name;
                        }
                    }
                    for(int info_idx = 0; info_idx < connect_info_table.size(); info_idx++)
                    {
                        string cmd = "";
                        for(int tmp_idx = 0; tmp_idx < cmd_pack[i].args.size(); tmp_idx++)
                        {
                            cmd += cmd_pack[i].args[tmp_idx];
                            cmd += " ";
                        }
                        cmd += ">";
                        cmd += to_string(cmd_pack[i].out_usr_id);
                        char tmp[100];
                        sprintf(tmp, "*** %s (#%d) just piped '%s' to %s (#%d) ***\n", 
                                connect_info_table[connect_info_idx].user_name.c_str(),
                                current_usr_id, temp.c_str(), receiver_name.c_str(),
                                cmd_pack[i].out_usr_id);
                        //if(connect_info_table[info_idx].socket_fd != socket_fd)
                        
                        write(connect_info_table[info_idx].socket_fd, tmp, strlen(tmp)); 
                    }
                }
            }
            else
            {
                int out_usr_exist = 0;
                for(int tmp_idx = 0; tmp_idx < connect_info_table.size();tmp_idx++)
                {
                    if(cmd_pack[i].out_usr_id == connect_info_table[tmp_idx].user_id)
                    {
                        out_usr_exist = 1;
                        break;
                    }
                }

                char tmp[100];
                if(out_usr_exist)
                {
                    sprintf(tmp, "*** Error: the pipe #%d->#%d already exists. ***\n", 
                        current_usr_id,cmd_pack[i].out_usr_id);
                }
                else
                {
                    sprintf(tmp, "*** Error: user #%d does not exist yet. ***\n", cmd_pack[i].out_usr_id);
                }
                
                write(socket_fd, tmp, strlen(tmp));
                cmd_no++;
                continue;
            }
        }
        else if(cmd_pack[i].type == "in_user_pipe")
        {
            if(usr_pipe_table[(cmd_pack[i].in_usr_id)-1][current_usr_id-1].in_fd == -1)
            {
                int in_usr_exist = 0;
                for(int tmp_idx = 0; tmp_idx < connect_info_table.size();tmp_idx++)
                {
                    if(cmd_pack[i].in_usr_id == connect_info_table[tmp_idx].user_id)
                    {
                        in_usr_exist = 1;
                        break;
                    }
                }

                char tmp[100];
                if(in_usr_exist)
                {
                    sprintf(tmp, "*** Error: the pipe #%d->#%d does not exists yet. ***\n", 
                            cmd_pack[i].in_usr_id,current_usr_id);
                }
                else
                {
                    sprintf(tmp, "*** Error: user #%d does not exist yet. ***\n", cmd_pack[i].in_usr_id);
                }
                write(socket_fd, tmp, strlen(tmp));
                int devNull = open("/dev/null", O_WRONLY);
                stdin_fd = devNull;
                cmd_no++;
                continue;
            }
            else
            {
                is_usr_target = 1;
                stdin_fd = usr_pipe_table[(cmd_pack[i].in_usr_id)-1][current_usr_id-1].in_fd;// target to current (in_fd = read end)
                close(usr_pipe_table[(cmd_pack[i].in_usr_id)-1][current_usr_id-1].out_fd);
                usr_pipe_table[(cmd_pack[i].in_usr_id)-1][current_usr_id-1].in_fd = -1;
                usr_pipe_table[(cmd_pack[i].in_usr_id)-1][current_usr_id-1].out_fd = -1;
                string sender_name = "";
                for(int tmp_idx = 0;tmp_idx < connect_info_table.size();tmp_idx++)
                {
                    if(connect_info_table[tmp_idx].user_id == cmd_pack[i].in_usr_id)
                    {
                        sender_name = connect_info_table[tmp_idx].user_name;
                    }
                }
                for(int info_idx = 0; info_idx < connect_info_table.size(); info_idx++)
                {
                    char tmp[50];
                    sprintf(tmp, "*** %s (#%d) just received from %s (#%d) ***\n", 
                            connect_info_table[connect_info_idx].user_name.c_str(),
                            current_usr_id, sender_name.c_str(), cmd_pack[i].in_usr_id);
                    //if(connect_info_table[info_idx].socket_fd != socket_fd)
                    
                    write(connect_info_table[info_idx].socket_fd, tmp, strlen(tmp)); 
                }
            }
        }
        else if(cmd_pack[i].type == "in_numpipe_user_pipe")
        {
            if(usr_pipe_table[(cmd_pack[i].in_usr_id)-1][current_usr_id-1].in_fd == -1)
            {
                int in_usr_exist = 0;
                for(int tmp_idx = 0; tmp_idx < connect_info_table.size();tmp_idx++)
                {
                    if(cmd_pack[i].in_usr_id == connect_info_table[tmp_idx].user_id)
                    {
                        in_usr_exist = 1;
                        break;
                    }
                }

                char tmp[100];
                if(in_usr_exist)
                {
                    sprintf(tmp, "*** Error: the pipe #%d->#%d does not exists yet. ***\n", 
                            cmd_pack[i].in_usr_id,current_usr_id);
                }
                else
                {
                    sprintf(tmp, "*** Error: user #%d does not exist yet. ***\n", cmd_pack[i].in_usr_id);
                }
            
                write(socket_fd, tmp, strlen(tmp));
                int devNull = open("/dev/null", O_WRONLY);
                stdin_fd = devNull;
                cmd_no++;
                continue;
            }
            else
            {
                is_usr_target = 1;
                stdin_fd = usr_pipe_table[(cmd_pack[i].in_usr_id)-1][current_usr_id-1].in_fd;// target to current (in_fd = read end)
                close(usr_pipe_table[(cmd_pack[i].in_usr_id)-1][current_usr_id-1].out_fd);
                usr_pipe_table[(cmd_pack[i].in_usr_id)-1][current_usr_id-1].in_fd = -1;
                usr_pipe_table[(cmd_pack[i].in_usr_id)-1][current_usr_id-1].out_fd = -1;
                string sender_name = "";
                for(int tmp_idx = 0;tmp_idx < connect_info_table.size();tmp_idx++)
                {
                    if(connect_info_table[tmp_idx].user_id == cmd_pack[i].in_usr_id)
                    {
                        sender_name = connect_info_table[tmp_idx].user_name;
                    }
                }
                for(int info_idx = 0; info_idx < connect_info_table.size(); info_idx++)
                {
                    char tmp[50];
                    sprintf(tmp, "*** %s (#%d) just received from %s (#%d) ***\n", 
                            connect_info_table[connect_info_idx].user_name.c_str(),
                            current_usr_id, sender_name.c_str(), cmd_pack[i].in_usr_id);
                    //if(connect_info_table[info_idx].socket_fd != socket_fd)
                    
                    write(connect_info_table[info_idx].socket_fd, tmp, strlen(tmp)); 
                }
            }

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
        else if(cmd_pack[i].type == "in_pipe_user_pipe")
        {
            if(usr_pipe_table[(cmd_pack[i].in_usr_id)-1][current_usr_id-1].in_fd == -1)
            {
                int in_usr_exist = 0;
                for(int tmp_idx = 0; tmp_idx < connect_info_table.size();tmp_idx++)
                {
                    if(cmd_pack[i].in_usr_id == connect_info_table[tmp_idx].user_id)
                    {
                        in_usr_exist = 1;
                        break;
                    }
                }

                char tmp[100];
                if(in_usr_exist)
                {
                    sprintf(tmp, "*** Error: the pipe #%d->#%d does not exists yet. ***\n", 
                            cmd_pack[i].in_usr_id,current_usr_id);
                }
                else
                {
                    sprintf(tmp, "*** Error: user #%d does not exist yet. ***\n", cmd_pack[i].in_usr_id);
                }
                write(socket_fd, tmp, strlen(tmp));
                int devNull = open("/dev/null", O_WRONLY);
                stdin_fd = devNull;
                cmd_no++;
                //continue;
            }
            else
            {
                is_usr_target = 1;
                stdin_fd = usr_pipe_table[(cmd_pack[i].in_usr_id)-1][current_usr_id-1].in_fd;// target to current (in_fd = read end)
                close(usr_pipe_table[(cmd_pack[i].in_usr_id)-1][current_usr_id-1].out_fd);
                usr_pipe_table[(cmd_pack[i].in_usr_id)-1][current_usr_id-1].in_fd = -1;
                usr_pipe_table[(cmd_pack[i].in_usr_id)-1][current_usr_id-1].out_fd = -1;
                string sender_name = "";
                for(int tmp_idx = 0;tmp_idx < connect_info_table.size();tmp_idx++)
                {
                    if(connect_info_table[tmp_idx].user_id == cmd_pack[i].in_usr_id)
                    {
                        sender_name = connect_info_table[tmp_idx].user_name;
                    }
                }
                for(int info_idx = 0; info_idx < connect_info_table.size(); info_idx++)
                {
                    char tmp[50];
                    sprintf(tmp, "*** %s (#%d) just received from %s (#%d) ***\n", 
                            connect_info_table[connect_info_idx].user_name.c_str(),
                            current_usr_id, sender_name.c_str(), cmd_pack[i].in_usr_id);
                    //if(connect_info_table[info_idx].socket_fd != socket_fd)
                    
                    write(connect_info_table[info_idx].socket_fd, tmp, strlen(tmp)); 
                }
            }

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
        
        //--------------------------------------------------

        
        //--Fork child to execute the command---------------
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

            execute_cmd(cmd_pack[i].args, socket_fd);
            exit(0);
        }
        else
        {
            int lineEndsWithPipeN = 0;
            if(cmd_pack[cmd_pack.size()-1].type == "num_pipe" or cmd_pack[cmd_pack.size()-1].type == "err_num_pipe")
            {
                lineEndsWithPipeN = 1;
            }

            if(is_target || is_usr_target)
            {
                //close(target_infd[0]);
                close(stdin_fd);
            }

            if((!lineEndsWithPipeN && cmd_pack[cmd_pack.size()-1].type != "out_user_pipe" && cmd_pack[cmd_pack.size()-1].type != "in_out_user_pipe") && (i == cmd_pack.size()-1))
            {
                int child_status;
                waitpid(pid, &child_status, 0);
            }
        }
        //---------------------------------------------------
        cmd_no++;
    }
}


int main(int argc, char *argv[])
{
    setenv("PATH", "bin:.", 1);
    int id = 1;
    int opt = 1;
    vector <int> client_socket(30);
    int master_socket , addrlen , new_socket ,  
          max_clients = 30 , activity, i , valread , sd;   
    int max_sd;   
    struct sockaddr_in address;   
         
    char buffer[1025];  //data buffer of 1K  
         
    //set of socket descriptors  
    fd_set readfds;   
     
    //initialise all client_socket[] to 0 so not checked  
    for (i = 0; i < max_clients; i++)   
    {   
        client_socket[i] = 0;   
    }   
    
    //create a master socket  
    if( (master_socket = socket(AF_INET , SOCK_STREAM , 0)) == 0)   
    {   
        perror("socket failed");   
        exit(EXIT_FAILURE);   
    }   
     
    //set master socket to allow multiple connections ,  
    //this is just a good habit, it will work without this  
    if( setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt,  
          sizeof(opt)) < 0 )   
    {   
        perror("setsockopt");   
        exit(EXIT_FAILURE);   
    }   
     
    if(argc < 2)
    {
        perror("Port not found");
        exit(EXIT_FAILURE);
    }
    int PORT = atoi(argv[1]);
    
    //type of socket created  
    address.sin_family = AF_INET;   
    address.sin_addr.s_addr = INADDR_ANY;   
    address.sin_port = htons( PORT );   
         
    //bind the socket to localhost port 8888  
    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address))<0)   
    {   
        perror("bind failed");   
        exit(EXIT_FAILURE);   
    }   
    printf("Listener on port %d \n", PORT);   
         
    //try to specify maximum of 3 pending connections for the master socket  
    if (listen(master_socket, 3) < 0)   
    {   
        perror("listen");   
        exit(EXIT_FAILURE);   
    }   
         
    //accept the incoming connection  
    
    addrlen = sizeof(address);   
    printf("Waiting for connections ...\n");   
    
    vector <connection_info> connect_info_table;
    vector <user_pipe> one_dim_usr_pipe(30);
    vector <vector <user_pipe> > usr_pipe_table;
    for(int usr_id = 0; usr_id < 30; usr_id++)// Initialize usr_pipe_table (i to j usr_pipe)
    {
        usr_pipe_table.push_back(one_dim_usr_pipe);
    }

    while(1)   
    {   
        //a message 
        string message = 
"***************************************\n\
** Welcome to the information server. **\n\
***************************************\n\
*** User '(no name)' entered from ";
        //clear the socket set  
        FD_ZERO(&readfds);   
     
        //add master socket to set  
        FD_SET(master_socket, &readfds);   
        max_sd = master_socket;   
             
        //add child sockets to set  
        for ( i = 0 ; i < max_clients ; i++)   
        {   
            //socket descriptor  
            sd = client_socket[i];   
                 
            //if valid socket descriptor then add to read list  
            if(sd > 0)   
                FD_SET( sd , &readfds);   
                 
            //highest file descriptor number, need it for the select function  
            if(sd > max_sd)   
                max_sd = sd;   
        }   
     
        //wait for an activity on one of the sockets , timeout is NULL ,  
        //so wait indefinitely 
        

        while((activity = select( max_sd + 1 , &readfds , NULL , NULL , NULL)) < 0);
        // tryagain:
        // activity = select( max_sd + 1 , &readfds , NULL , NULL , NULL);   
       
        // if ((activity < 0))   
        // {   
        //     perror("select error");   
        //     goto tryagain;
        // }      
        //If something happened on the master socket ,  
        //then its an incoming connection  
        if (FD_ISSET(master_socket, &readfds))   
        {   
            if ((new_socket = accept(master_socket,  
                    (struct sockaddr *)&address, (socklen_t*)&addrlen))<0)   
            {   
                perror("accept");   
                exit(EXIT_FAILURE);   
            }   
             
            //inform user of socket number - used in send and receive commands  
            printf("New connection , socket fd is %d , ip is : %s , port : %d \n" ,
                     new_socket , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));
            
            //--Record every socket's cmd no------------------
            for(int id_num = 1; id_num <= 30; id_num ++)
            {
                int id_been_used = 0;
                for(int table_idx = 0; table_idx < connect_info_table.size(); table_idx++)
                {
                    if(id_num == connect_info_table[table_idx].user_id)
                    {
                        id_been_used = 1;
                        break;
                    }
                }
                if(!id_been_used)
                {
                    id = id_num;
                    break;
                }
            }
            struct connection_info new_connect_info;
            new_connect_info.socket_fd = new_socket;
            new_connect_info.cmd_no = 1;
            new_connect_info.user_id = id;
            new_connect_info.user_name = "(no name)";
            new_connect_info.ip_port = string(inet_ntoa(address.sin_addr)) + ":" + to_string(ntohs(address.sin_port));
            new_connect_info.numpipe_table = {};
            map <string, string> env;
            env["PATH"] = "bin:.";
            new_connect_info.env = env;
            connect_info_table.push_back(new_connect_info);
            for(int info_idx = 0; info_idx < connect_info_table.size(); info_idx++)
            {
                char tmp[100];
                sprintf(tmp, "*** User '%s' entered from %s. ***\n", 
                        new_connect_info.user_name.c_str(),new_connect_info.ip_port.c_str());
                if(connect_info_table[info_idx].socket_fd != new_socket)
                {
                    write(connect_info_table[info_idx].socket_fd, tmp, strlen(tmp));
                }
                
                
            }
            //------------------------------------------------

            string wellcome_message = message;
            wellcome_message += string(inet_ntoa(address.sin_addr));
            wellcome_message += ":";
            wellcome_message += to_string(ntohs(address.sin_port));
            wellcome_message += ". ***\n";
            
            //send new connection greeting message  
            if(send(new_socket, wellcome_message.c_str(), strlen(wellcome_message.c_str()), 0) 
                != strlen(wellcome_message.c_str()))   
            {   
                perror("send");   
            }
            send(new_socket , "% " , 2 , 0);
            printf("Welcome message sent successfully\n");   
                 
            //add new socket to array of sockets  
            for (i = 0; i < max_clients; i++)   
            {   
                //if position is empty  
                if( client_socket[i] == 0 )   
                {   
                    client_socket[i] = new_socket;   
                    printf("Adding to list of sockets as %d\n" , i);   
                    break;   
                }   
            }   
        }        
        //else its some IO operation on some other socket 
        for (i = 0; i < max_clients; i++)   
        {   
            sd = client_socket[i];   
                 
            if(FD_ISSET(sd , &readfds))   
            {   
                //Check if it was for closing , and also read the  
                //incoming message
                int connect_info_idx = 0;
                for(connect_info_idx = 0; connect_info_idx < connect_info_table.size(); connect_info_idx++)
                {
                    if (sd == connect_info_table[connect_info_idx].socket_fd)
                    {
                        break;
                    }
                }
                
                if ((valread = read( sd , buffer, 1024)) == 0)   
                {   
                    //Somebody disconnected , get his details and print  
                    getpeername(sd , (struct sockaddr*)&address , (socklen_t*)&addrlen);   
                    printf("Host disconnected , ip %s , port %d \n" ,  
                          inet_ntoa(address.sin_addr) , ntohs(address.sin_port));   
                        
                    //Close the socket and mark as 0 in list for reuse
                    connect_info_table.erase(connect_info_table.begin()+connect_info_idx);//Erase socket info  
                    close(sd);
                    client_socket[i] = 0;   
                }   
                //Echo back the message that came in  
                else 
                {   
                    //set the string terminating NULL byte on the end  
                    //of the data read
                    string line = "";
                    if(valread >= 1024)
                    {
                        string temp(buffer);
                        line += temp;
                        while(valread != 0)
                        {
                            valread = read( sd , buffer, 1024);
                            if(valread < 1024)
                            {
                                buffer[valread] = '\0';
                                line += string(buffer);
                                break;
                            }
                            line += string(buffer);
                        }
                    }
                    else
                    {
                        buffer[valread] = '\0';
                        string temp(buffer);
                        line += temp;   
                    }
                
                    // buffer[valread] = '\0';
                    // string line(buffer);
                    //printf("READ: %s \n", line.c_str());
                    exe_shell_cmd(sd, connect_info_table[connect_info_idx].cmd_no,
                                 connect_info_table[connect_info_idx].numpipe_table, 
                                 line, connect_info_table, usr_pipe_table, client_socket,
                                 i);

                    send(sd , "% " , 2 , 0);
                }   
            }   
        }  
    }
    return 0;
}