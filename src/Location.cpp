#include "../inc/webserv.hpp"
#include "Location.hpp"


Location::Location()
{
    _autoindex = false;
    _upload = false;
    _cgi = false;
}

Location::~Location()
{
}

void Location::setIndexs(string const &buff)
{
    _indexs.push_back(buff);
}

void Location::setRoot(string const &buff)
{
    _root = buff;
}

void Location::setAutoindex(bool flag)
{
    _autoindex = flag;
}

void Location::setUpload(bool flag)
{
    _upload = flag;
}

void Location::setCgi(bool flag)
{
    _cgi = flag;
}

void Location::setUploadPath(string const &buff)
{
    _upload_path = buff;
}

void Location::setCgiPath(string const &buff1, string const &buff2)
{
    _cgi_path[buff1] = buff2;
}

void Location::setReturn(string const &code, string const &buff)
{
    string codes[6] = {"200", "201", "301", "400", "403", "404"};
    for (int i = 0; i < 6; i++)
        if (code == codes[i])
        {
            _return.first = code;
            _return.second = buff;
            return;
        }
    throw runtime_error(ERR "Invalid return code");
}

void Location::setMethods(string const &buff)
{
    string method[3] = {"GET", "POST", "DELETE"};
    for (int i = 0; i < 3; i++)
        if (buff == method[i])
        {
            _methods.push_back(buff);
            return;
        }
    throw runtime_error(ERR "Invalid method");
}
