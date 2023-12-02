#pragma once
#include "webserv.hpp"
#include "Location.hpp"

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
