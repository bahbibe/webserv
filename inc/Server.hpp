#pragma once
#include "webserv.hpp"
#include "Location.hpp"
typedef struct s_direrctive
{
    int host;
    int listen;
    int server_name;
    int index;
    int loc_index;
    int root;
    int loc_root;
    int autoindex;
    int loc_autoindex;
    int client_max_body_size;
    int cgi;
    int upload;
    int upload_path;
    int allow;
    int return_code;

} t_dir;


class Server
{
private:
    string _host;
    string _port;
    vector<string> _server_names;
    vector<string> _indexs;
    string _server_root;
    map<string, string> _error_pages;
    string _client_max_body_size;
    bool _autoindex;
    map<string, Location *> _locations;
    t_dir _dir;

public:
    Server();
    ~Server();
    void parseConfig(string const &);
    void parseServer(string const &);
    Location *parseLocation(stringstream &ss);
    void setErrorCodes(string const &, string const &);
    void print();
};
