#pragma once
#include "webserv.hpp"

class Location
{
private:
    vector<string> _methods;
    vector<string> _indexs;
    string _root;
    bool _autoindex;
    bool _cgi;
    bool _upload;
    string _upload_path;
    map<string, string> _cgi_path;
    pair<string, string> _return;
    
public:
    Location();
    ~Location();
    void setMethods(string const &);
    void setIndexs(string const &);
    void setRoot(string const &);
    void setAutoindex(bool);
    void setUpload(bool);
    void setCgi(bool);
    void setUploadPath(string const &);
    void setCgiPath(string const &, string const &);
    void setReturn(string const &, string const &);
};