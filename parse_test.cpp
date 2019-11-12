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

struct command{
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
                                    cmd.type = "in_num_pipe";
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



void shell_loop()
{
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
            printf("cmd: %s\n", cmd_pack[i].args[0].c_str());
            printf("cmd_type: %s\n", cmd_pack[i].type.c_str());
            for(int tmp_idx = 0; tmp_idx < cmd_pack[i].args.size(); tmp_idx++)
            {
                printf("args[%d]: %s\n", tmp_idx, cmd_pack[i].args[tmp_idx].c_str());
            }
            printf("numpipe: %d\n", cmd_pack[i].num_pipe);
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