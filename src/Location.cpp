#include "../inc/webserv.hpp"
#include "../inc/Location.hpp"

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

void Location::setUploadPath(string const &buff)
{
    _upload_path = buff;
}

void Location::setCgi(bool flag)
{
    _cgi = flag;
}

void Location::setReturn(string const &buff)
{
    _return = buff;
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

void Location::print()
{
    cout << "    indexs: ";
    for (size_t i = 0; i < _indexs.size(); i++)
        cout << _indexs[i] << " ";
    cout << endl;
    cout << "    root: " << _root << endl;
    cout << "    autoindex: " << _autoindex << endl;
    cout << "    upload: " << _upload << endl;
    cout << "    cgi: " << _cgi << endl;
    cout << "    upload_path: " << _upload_path << endl;
    cout << "    return: " << _return << endl;
    cout << "    methods: ";
    for (size_t i = 0; i < _methods.size(); i++)
        cout << _methods[i] << " ";
    cout << endl;
}