#pragma once 
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
#define RED "\033[0;31m"
#define GREEN "\033[0;32m"
#define YELLOW "\033[0;33m"
#define BLUE "\033[0;34m"
#define RESET "\033[0m"
#define USAGE YELLOW "Usage: ./webserv [config_file] DEFAULT=NONE" RESET
#define ERR RED "Error: " RESET
#define DEFAULT_CONF "conf/default.conf"
#define DEFAULT_PORT "80"
#define MAX_EVENTS 1024
#define TIMEOUT 10
#define CLOCKWORK(x) double(clock() - x) / CLOCKS_PER_SEC
#define LISTENING GREEN "Listening on " RESET
#define BUFFER_SIZE 1024
// #include "Request.hpp"
// #include "Response.hpp"
// #include "Server.hpp"
using namespace std;


class Response;
class Request;
class Server;
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
    int cgi_upload_path;
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

extern t_events ep;

class Webserver
{
private:
    vector<Server> _servers;
    map<int, Request> _req;
    map<int, Response> _resp;
public:
    Webserver();
    ~Webserver();
    void brackets(string const &file);
    size_t serverCount();
    Server &operator[](size_t index);
    void start();
    void newConnection(map<int, Request> &req ,Server &server);
    void closeConnection(map<int, Request> &req, map<int, Response> &resp, int sock);
    bool timeoutAndErrors(map<int, Request> &req, map<int, Response> &resp, int sock);
    bool matchServer(map<int, Request> &req, int sock);
    class ServerException : public exception
    {
    private:
        string _msg;
    public:
        ServerException(string const &msg) : _msg(msg) {}
        virtual ~ServerException() throw() {}
        virtual const char *what() const throw(){ return _msg.c_str();}
    };
};

bool isWhitespace(string const&);
bool isComment(string const&);
bool isBrackets(string const&);
bool isServerDir(string const &);
bool isLocationDir(string const &);
bool isIpV4(string const &str);
bool isNumber(string const &);
bool duplicateDirective(t_dir dir);
bool allowedConfig(string const &line);
void trim(string &str);
