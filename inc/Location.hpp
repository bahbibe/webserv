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

public:
    t_dir _dir;
    Location();
    ~Location();
    void setMethods(string const &);
    void setIndexs(string const &);
    void setRoot(string const &);
    void setAutoindex(bool);
    void setUpload(bool);
    void setCgi(bool);
    void setUploadPath(string const &);
    void setCgiUploadPath(string const &);
    void setReturn(string const &);
    void print();

    string getReturn() const;
    vector<string> getMethods() const;
    string getRoot() const;
    bool getUpload() const;
    bool getAutoindex() const;
    bool getCgi() const;
    string getUploadPath() const;
    string getCgiUploadPath() const;
    vector<string> getIndexs() const;
};