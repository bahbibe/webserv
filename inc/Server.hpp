#pragma once 
#include "webserv.hpp"

class Location
{
private:
    vector<string> _methods;
    vector<string> _indexs;
    string _root;
    bool _autoindex;
    bool _upload;
    string _upload_path;
    bool _cgi;
    map<string, string> _cgi_path;
    string return_code;
public:
    Location();
    ~Location();
};

class Server
{
    private:
        vector<Location *> _locations;
        vector<string> _server_names;
        map<int, string> _error_pages;
        vector<string> _indexs;
        string _host;
        string _port;
        string _server_root;
        bool _autoindex;
        int _client_max_body_size;
    public:
        Server();
        ~Server();
};
