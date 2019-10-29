#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <cstring>
#include <fstream>
#include <unistd.h>
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


void shell_loop()
{
    string line;
    string const DELI{" "};
    while(true)
    {
        printf("> ");
        getline(cin, line);
        vector <string> tokens;
        tokens = parse_line(line, DELI);
        int is_builtin = check_builtin(tokens);    
    }
}


int main()
{
    setenv("PATH", "bin:.", 1);
    shell_loop();
    return 0;
}