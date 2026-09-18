#pragma once
#include "webserv.hpp"

class Location
{
private:
    bool _autoindex;
    bool _cgi;
    bool _upload;
    vector<string> _methods;
    vector<string> _indexs;
    string _root;
    string _upload_path;
    string _cgi_upload_path;
    string _return;
    map<string, string> _cgiPaths;
    size_t _client_max_body_size;

public:
    t_dir _dir;
    Location();
    ~Location();
    Location(Location const &src);
    Location &operator=(Location const &src);
    void setMethods(string const &);
    void setIndexs(string const &);
    void setRoot(string const &);
    void setAutoindex(bool);
    void setUpload(bool);
    void setCgi(bool);
    void setUploadPath(string const &);
    void setCgiUploadPath(string const &);
    void setReturn(string const &);
    void setCgiPath(string const &ext, string const &interpreter);
    void setClientMaxBodySize(size_t);
    // Directive application, called from ConfigParser - the same
    // per-directive validation Server::parseLocation() used to do
    // inline, just handed an already-tokenized directive name and
    // its same-line value tokens instead of scanning raw text
    // itself. Uses this->_dir directly for duplicate tracking, same
    // as the code it replaces did.
    void applyLocationDirective(string const &name, vector<string> const &values);
    // Runs once, right after a location block's closing brace: the
    // duplicate-directive check, and falling back to the owning
    // server's root/autoindex/client_max_body_size for anything this
    // location didn't set explicitly.
    void finalizeLocationDirectives(string const &serverRoot, bool serverAutoindex, size_t serverClientMaxBodySize);

    string getReturn() const;
    size_t getClientMaxBodySize() const;
    vector<string> getMethods() const;
    string getRoot() const;
    bool getUpload() const;
    bool getAutoindex() const;
    bool getCgi() const;
    string getUploadPath() const;
    string getCgiUploadPath() const;
    vector<string> getIndexs() const;
    map<string, string> getCgiPaths() const;
};