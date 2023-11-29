#pragma once 
#include "webserv.hpp"

typedef struct s_direrctive
{
    int listen;
    int server_name;
    int error_page;
    int index;
    int root;
    int autoindex;
    int client_max_body_size;
    int cgi;
    int upload;
}t_dir;

class Location
{
private:
    vector<string> _methods;
    vector<string> _indexs;
    map<string, string> _cgi_path;
    string _root;
    string _upload_path;
    string return_code;
    bool _autoindex;
    bool _upload;
    bool _cgi;
public:
    Location();
    ~Location();
    // void parseLocation(string const &);
};

class Server
{
    private:
        t_dir _dir;
        map<int, string> _error_pages;
        vector<Location *> _locations;
        vector<string> _server_names;
        vector<string> _indexs;
        string _host;
        string _port;
        string _server_root;
        bool _autoindex;
        unsigned int _client_max_body_size;
    public:
        Server();
        ~Server();
        void parseServer(string const &);
        void print();
};
