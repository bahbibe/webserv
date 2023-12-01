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
    string _return;

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
    void setReturn(string const &);
    void print();
};