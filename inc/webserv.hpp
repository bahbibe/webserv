#pragma once 
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <stack>
#include <map>
// #include <stdexcept>
#include <algorithm>
// #include <cctype>
#include <cstring>
#include <cstdlib>
// #include <sys/stat.h>
// #include <sys/types.h>
// #include <unistd.h>
// #include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
using namespace std;
typedef struct s_direrctive
{
    int host;
    int listen;
    int server_name;
    int index;
    int root;
    int autoindex;
    int client_max_body_size;
    int cgi;
    int upload;
    int upload_path;
    int allow;
    int return_code;
} t_dir;

// struct sockaddr_in {
//     short            sin_family;   // e.g. AF_INET, AF_INET6
//     unsigned short   sin_port;     // e.g. htons(3490)
//     struct in_addr   sin_addr;     // see struct in_addr, below
//     char             sin_zero[8];  // zero this if you want to
// };

bool isWhitespace(string const&);
bool isComment(string const&);
bool isBrackets(string const&);
bool isServerDir(string const &);
bool isLocationDir(string const &);
bool isIpV4(string const &str);
bool isNumber(string const &);
void brackets(string const &file);
bool duplicateDirective(t_dir dir);

/*MACROS*/

#define RED "\033[0;31m"
#define GREEN "\033[0;32m"
#define YELLOW "\033[0;33m"
#define BLUE "\033[0;34m"
#define RESET "\033[0m"
#define USAGE YELLOW "Usage: ./webserv [config_file] if no config file is provided, default.conf will be used" RESET
#define OPEN_BR "{"
#define CLOSE_BR "}"
#define COLON ":"
#define SEMICOLON ";"
#define ERR RED "Error: " RESET