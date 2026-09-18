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
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <netdb.h>
#include <csignal>
#include <utility>
#include <pwd.h>
#include <grp.h>
#include <spdlog/spdlog.h>
#define RED "\033[0;31m"
#define YELLOW "\033[0;33m"
#define RESET "\033[0m"
#define USAGE YELLOW "Usage: ./webserv [config_file] DEFAULT=NONE" RESET
#define ERR RED "Error: " RESET
#define DEFAULT_CONF "default.conf"
// Installed-system config location: preferred over the
// binary-relative conf/ fallback above whenever it exists, for both
// the auto-selected config file and mime.types (Server::mimeTypes()
// reads confDir + "mime.types").
#define SYSTEM_CONF_DIR "/etc/webserv/"
#define SYSTEM_CONF_FILE "webserv.conf"
#define SYSTEM_LOG_DIR "/var/log/webserv/"
#define DEFAULT_PORT "80"
#define MAX_EVENTS 1024
#define MAX_CONNECTIONS 512
#define TIMEOUT 10
#define REQUEST_TIMEOUT 30
#define SHUTDOWN_GRACE 5
#define CGI_TIMEOUT 5
#define CLOCKWORK(x) double(time(NULL) - (x))
#define BUFFER_SIZE 1024
#define MAX_HEADER_BYTES 8192
using namespace std;


class Response;
class Request;
class Server;

class WebservException : public exception
{
public:
    explicit WebservException(string const &msg) : _msg(msg) {}
    const char *what() const noexcept override { return _msg.c_str(); }

private:
    string _msg;
};

// Owns exactly one fd (socket, pipe, ...) and close()s it on destruction,
// on reset(), or when overwritten by move-assignment. Move-only - a raw
// int cached elsewhere (e.g. Server::_socket) stays a non-owning view.
class UniqueFd
{
public:
    UniqueFd() noexcept : _fd(-1) {}
    explicit UniqueFd(int fd) noexcept : _fd(fd) {}
    ~UniqueFd() { reset(); }

    UniqueFd(UniqueFd const &) = delete;
    UniqueFd &operator=(UniqueFd const &) = delete;

    UniqueFd(UniqueFd &&other) noexcept : _fd(other._fd) { other._fd = -1; }
    UniqueFd &operator=(UniqueFd &&other) noexcept
    {
        if (this != &other)
        {
            reset();
            _fd = other._fd;
            other._fd = -1;
        }
        return *this;
    }

    int get() const noexcept { return _fd; }
    bool valid() const noexcept { return _fd != -1; }

    void reset(int fd = -1) noexcept
    {
        if (_fd != -1)
            close(_fd);
        _fd = fd;
    }

private:
    int _fd;
};

// Collects every config-validation problem found across the whole
// file (all server/location blocks) instead of stopping at the
// first one - see the "report all config errors" behavior.
class ConfigValidator
{
public:
    void add(string const &msg) { _errors.push_back(msg); }
    bool hasErrors() const { return !_errors.empty(); }
    size_t errorCount() const { return _errors.size(); }
    vector<string> const &errors() const { return _errors; }
    // Never called by the running server (one config is validated
    // once per process lifetime) - exists so unit tests can start each
    // parser test case from a clean slate against the shared global
    // configErrors, instead of errors accumulating across every test
    // that's run before it in the same test binary.
    void clear() { _errors.clear(); }
    void report(ostream &out) const
    {
        out << RED "Config has " << _errors.size() << " error(s):" RESET "\n";
        for (size_t i = 0; i < _errors.size(); i++)
            out << "  " << _errors[i] << "\n";
    }

private:
    vector<string> _errors;
};

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
    int ssl_certificate;
    int ssl_certificate_key;
} t_dir;

typedef struct s_events
{
    int epollFd;
    struct epoll_event events[MAX_EVENTS];
    struct epoll_event event;
} t_events;

extern t_events ep;
extern map<string, UniqueFd> socketMap;
extern string confDir;
extern string accessLogPath;
// Main-context directives, parsed and applied via ConfigParser:
// written outside any server {} block. Empty pidPath/errorLogPath/
// dropUser means the directive wasn't given - no pidfile is written,
// diagnostics stay on the default stdout sink, no privilege drop
// happens (see Privileges.hpp).
extern string pidPath;
extern string errorLogPath;
extern string errorLogLevel;
extern string dropUser;
extern string dropGroup;
extern volatile sig_atomic_t g_shutdown;
extern volatile sig_atomic_t g_reopenLog;
extern ConfigValidator configErrors;
void handleShutdownSignal(int signum);
void handleReopenLogSignal(int signum);

class Webserver
{
private:
    map<int, Request> _req;
    map<int, Response> _resp;
    // CGI pipe fd -> owning client socket fd. Populated when Response::CGI()
    // creates a pipe, consulted by the event loop to route a pipe-fd epoll
    // event to the right Request/Response pair (pipe events are driven
    // independently of the client socket's own events).
    map<int, int> _cgiFdToClient;
    ofstream _accessLog;
    void stopListening();
    void logAccess(Request &req, Response *resp);
    void safeCloseConnection(int sock);
    void reopenAccessLog();
public:
    vector<Server> _servers;
    Webserver();
    ~Webserver();
    Server &operator[](size_t index);
    void start();
    void newConnection(map<int, Request> &req, Server &server);
    void closeConnection(map<int, Request> &req, map<int, Response> &resp, int sock);
    bool matchServer(map<int, Request> &req, int sock);
};

bool isWhitespace(string const&);
bool isComment(string const&);
bool isServerDir(string const &);
bool isLocationDir(string const &);
int resolveHostFamily(string const &host);
bool isNumber(string const &);
bool duplicateDirective(t_dir dir);
void trim(string &str);
