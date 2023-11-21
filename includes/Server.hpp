#pragma once 
#include "webserv.hpp"

class Location
{
private:
    string _root;
    string _index;
    string _autoindex;
    string _cgi;
    string _upload;
    string _client_max_body_size;
    string _error_page;
public:
    Location();
    ~Location();
};

class Server
{
    private:
        Location _location;
    public:
        Server();
        ~Server();
};
