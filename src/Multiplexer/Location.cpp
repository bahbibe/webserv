#include "../../inc/webserv.hpp"
#include "../../inc/Location.hpp"
#include "../../inc/Server.hpp"

Location::Location() : _autoindex(false), _cgi(false), _upload(false)
{
    memset(&_dir, 0, sizeof(_dir));
}
Location::~Location(){}
void Location::setIndexs(string const &buff){_indexs.push_back(buff);}
void Location::setRoot(string const &buff) {_root = buff;}
void Location::setAutoindex(bool flag) {_autoindex = flag;}
void Location::setUpload(bool flag) {_upload = flag;}
void Location::setUploadPath(string const &buff) {_upload_path = buff;}
void Location::setCgiUploadPath(string const &buff) {_cgi_upload_path = buff;}
void Location::setCgi(bool flag) {_cgi = flag;}
void Location::setReturn(string const &buff) {_return = buff;}
string Location::getReturn() const { return _return; }
vector<string> Location::getMethods() const { return _methods; }
string Location::getRoot() const { return _root; }
bool Location::getUpload() const { return _upload; }
bool Location::getAutoindex() const { return _autoindex; }
bool Location::getCgi() const { return _cgi; }
string Location::getUploadPath() const { return _upload_path; }
vector<string> Location::getIndexs() const { return _indexs; }
string Location::getCgiUploadPath() const { return _cgi_upload_path; }

void Location::setMethods(string const &buff)
{
    string method[3] = {"GET", "POST", "DELETE"};
    for (int i = 0; i < 3; i++)
        if (buff == method[i])
        {
            _methods.push_back(buff);
            return;
        }
    throw Server::ServerException(ERR "Invalid method");
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