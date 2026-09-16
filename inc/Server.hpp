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
public:
    Server();
    Server(Server const &src);
    Server &operator=(Server const &src);
    void mimeTypes();
    void setErrorCodes(string const &, string const &);
    // Directive application (see V4-PLAN.md Phase 4's ConfigParser,
    // which owns tokenizing/block structure and hands each already-
    // recognized directive's name and same-line value tokens here -
    // this is the same validation every directive already had, just
    // no longer doing its own text scanning to get called).
    void applyServerDirective(string const &name, vector<string> const &values, t_dir &dir);
    // Runs once, right after a server block's closing brace: the
    // duplicate-directive check, and the "listen ... ssl needs both
    // cert files" check that can only be evaluated once the whole
    // block is known.
    void finalizeServerDirectives(t_dir const &dir);
    void addLocation(string const &path, unique_ptr<Location> location);
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
