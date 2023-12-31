#pragma once
#include "webserv.hpp"
#include "Location.hpp"
// #include "Request.hpp"
// #include "Response.hpp"

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
    int _socket;
    static streampos _pos;
public:
    Server();
    ~Server();
    void parseServer(string const &);
    Location *parseLocation(stringstream &ss);
    void setErrorCodes(string const &, string const &);
    void print();
    void setupSocket();
    int getSocket() const;
    class ServerException : public exception
    {
    private:
        string _msg;
    public:
        ServerException(string const &msg) : _msg(msg) {}
        virtual ~ServerException() throw() {}
        virtual const char *what() const throw(){ return _msg.c_str();}
    };

    size_t getClientMaxBodySize() const;
    map<string, Location *> getLocations() const;
    string getHost() const;
    string getPort() const;
    string getRoot() const;
    bool getAutoindex() const;
    map<string, string> getErrorPages() const;
    vector<string> getIndexs() const;
    vector<string> getServerNames() const;
};
