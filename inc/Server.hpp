#pragma once
#include "webserv.hpp"
#include "Location.hpp"
#include <memory>

class Server
{
private:
    map<string, unique_ptr<Location> > _locations;
    map<string, string> _error_pages;
    map<string, vector<string> > _extensions;
    map<string, string> _types;
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
    Server(Server const &src);
    Server &operator=(Server const &src);
    void parseServer(string const &);
    void mimeTypes();
    unique_ptr<Location> parseLocation(stringstream &ss);
    void setErrorCodes(string const &, string const &);
    void setupSocket();
    int getSocket() const;
    string addrKey() const;

    size_t getClientMaxBodySize() const;
    const map<string, unique_ptr<Location> > &getLocations() const;
    string getHost() const;
    string getPort() const;
    string getRoot() const;
    bool getAutoindex() const;
    map<string, string> getErrorPages() const;
    vector<string> getIndexs() const;
    vector<string> getServerNames() const;
    map<string, vector<string> > getExtensions() const;
    map<string, string> getTypes() const;
};
