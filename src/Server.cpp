#include "../inc/Server.hpp"

Server::Server()
{
    _autoindex = false;
}

Server::~Server()
{
    map<string, Location *>::iterator it = _locations.begin();
    for (; it != _locations.end(); it++)
    {
        delete it->second;
    }
}

void Server::setErrorCodes(string const &code, string const &buff)
{
    string codes[11] = {"400", "403", "404", "405", "411", "413", "414", "500", "501", "503", "505"};
    for (int i = 0; i < 11; i++)
        if (code == codes[i])
        {
            _error_pages[code] = buff;
            return;
        }
    throw runtime_error(ERR "Invalid error code");
}

void Server::print()
{
    cout << "==================SERVER==================\n";
    cout << "host: " + _host << "\n";
    cout << "port: " + _port << "\n";
    cout << "server_names: " << "\n";
    for (vector<string>::iterator it = _server_names.begin(); it != _server_names.end(); it++)
        cout << "\t" << *it << "\n";
    cout << "indexs: \n";
    for (vector<string>::iterator it = _indexs.begin(); it != _indexs.end(); it++)
        cout << "\t" << *it << "\n";
    cout << "server_root: " + _server_root << "\n";
    cout << "error_pages: " << "\n";
    for (map<string, string>::iterator it = _error_pages.begin(); it != _error_pages.end(); it++)
        cout << "\t" << it->first << " " << it->second << " " << "\n";
    cout << "client_max_body_size: " << _client_max_body_size << "\n";
    cout << "autoindex: " << _autoindex << "\n";
    cout << "==================LOCATIONS==================\n";
    for (map<string, Location *>::iterator it = _locations.begin(); it != _locations.end(); it++)
    {
        cout << "Location: " << it->first << "\n";
        it->second->print();
    }
}
