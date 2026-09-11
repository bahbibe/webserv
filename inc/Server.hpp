#pragma once
#include "webserv.hpp"
#include "Location.hpp"
#include "Tls.hpp"
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
    bool _ssl;
    string _sslCertPath;
    string _sslKeyPath;
    // Shared (not unique) across copies of this Server: config parsing
    // copies Server objects around (push_back/operator=) before the
    // context is ever loaded, and once setupSsl() has run on the
    // instance actually kept in Webserver::_servers, every accepted
    // connection just needs read access to the same loaded context.
    shared_ptr<SSL_CTX> _sslCtx;
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
    void setupSsl();
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
    bool getSsl() const;
    SSL_CTX *getSslCtx() const;
};
