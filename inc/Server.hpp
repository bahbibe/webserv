#pragma once
#include "webserv.hpp"
#include "Location.hpp"

class Server
{
private:
    map<string, Location *> _locations;
    map<string, string> _error_pages;
    vector<string> _server_names;
    vector<string> _indexs;
    string _host;
    string _port;
    string _server_root;
    string _client_max_body_size;
    bool _autoindex;
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
