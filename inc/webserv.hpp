#pragma once 
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
#define DEFAULT_CONF "conf/default.conf"
#define DEFAULT_PORT "80"
#define MAX_EVENTS 1024
#define LISTENING GREEN "Listening on " RESET
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <vector>
#include <stack>
#include <map>
#include <cstring>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
// #include <sys/stat.h>
// #include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>

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
    int server;
} t_dir;

typedef struct s_events
{
    int epollFd;
    struct epoll_event events[MAX_EVENTS];
    struct epoll_event event;
} t_events;


bool isWhitespace(string const&);
bool isComment(string const&);
bool isBrackets(string const&);
bool isServerDir(string const &);
bool isLocationDir(string const &);
bool isIpV4(string const &str);
bool isNumber(string const &);
void brackets(string const &file);
bool duplicateDirective(t_dir dir);
