#include "../../inc/Server.hpp"
#include "../../inc/Response.hpp"

streampos Server::_pos = 0;
Server::Server() : _autoindex(false)
{
    memset(&_dir, 0, sizeof(_dir));
}


Server::Server(Server const &src)
{
    *this = src;
}

Server &Server::operator=(Server const &src)
{
    if (this != &src)
    {
        _locations.clear();
        map<string, unique_ptr<Location> >::const_iterator it = src._locations.begin();
        for (; it != src._locations.end(); it++)
        {
            _locations[it->first] = make_unique<Location>(*it->second);
        }
        _error_pages = src._error_pages;
        _extensions = src._extensions;
        _types = src._types;
        _server_names = src._server_names;
        _indexs = src._indexs;
        _host = src._host;
        _port = src._port;
        _server_root = src._server_root;
        _client_max_body_size = src._client_max_body_size;
        _autoindex = src._autoindex;
        _socket = src._socket;
    }
    return *this;
}

const map<string, unique_ptr<Location> > &Server::getLocations() const
{
    return _locations;
}

string Server::getHost() const
{
    return _host;
}

string Server::getPort() const
{
    return _port;
}

string Server::getRoot() const
{
    return _server_root;
}

bool Server::getAutoindex() const
{
    return _autoindex;
}

map<string, string> Server::getErrorPages() const
{
    return _error_pages;
}

vector<string> Server::getIndexs() const
{
    return _indexs;
}

vector<string> Server::getServerNames() const
{
    return _server_names;
}

map<string, vector<string> > Server::getExtensions() const
{
    return _extensions;
}

map<string, string> Server::getTypes() const
{
    return _types;
}

int Server::getSocket() const
{
    return _socket;
}

size_t Server::getClientMaxBodySize() const
{
    if (_client_max_body_size == "")
        return 0;
    size_t size = 0;
    stringstream ss(_client_max_body_size);
    ss >> size;
    return size;
}
void Server::setErrorCodes(string const &code, string const &buff)
{
    string codes[9] = {"400", "403", "404", "405", "409", "413", "414", "500", "501"};
    for (int i = 0; i < 9; i++)
        if (code == codes[i])
        {
            _error_pages[code] = buff;
            return;
        }
    addConfigError(ERR "Invalid error_page code: " + code);
}

void Server::print()
{
    cout << "==================SERVER==================\n";
    cout << "host: " + _host << "\n";
    cout << "port: " + _port << "\n";
    cout << "server_names: "
            << "\n";
    for (vector<string>::iterator it = _server_names.begin(); it != _server_names.end(); it++)
        cout << "\t" << *it << "\n";
    cout << "indexs: \n";
    for (vector<string>::iterator it = _indexs.begin(); it != _indexs.end(); it++)
        cout << "\t" << *it << "\n";
    cout << "server_root: " + _server_root << "\n";
    cout << "error_pages: "
            << "\n";
    for (map<string, string>::iterator it = _error_pages.begin(); it != _error_pages.end(); it++)
        cout << "\t" << it->first << " " << it->second << " "
                << "\n";
    cout << "client_max_body_size: " << _client_max_body_size << "\n";
    cout << "autoindex: " << _autoindex << "\n";
    cout << "==================LOCATIONS==================\n";
    for (map<string, unique_ptr<Location> >::iterator it = _locations.begin(); it != _locations.end(); it++)
    {
        cout << "Location: " << it->first << "\n";
        it->second->print();
    }
}

string Server::addrKey() const
{
    if (_host.find(':') != string::npos)
        return "[" + _host + "]:" + _port;
    return _host + ":" + _port;
}

void Server::setupSocket()
{
    string key = addrKey();
    map<string, int>::iterator it = socketMap.find(key);
    if (it != socketMap.end())
    {
        _socket = it->second;
        return;
    }
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV | AI_PASSIVE;
    int gaiStatus = getaddrinfo(_host.c_str(), _port.c_str(), &hints, &res);
    if (gaiStatus != 0)
    {
        addConfigError(ERR "Invalid host/port " + key + " (" + gai_strerror(gaiStatus) + ")");
        return;
    }
    int sockOpt = 1;
    if ((_socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1)
    {
        addConfigError(ERR "Failed to create socket for " + key);
        freeaddrinfo(res);
        return;
    }
    if (fcntl(_socket, F_SETFL, O_NONBLOCK) == -1)
    {
        addConfigError(ERR "Failed to set socket non-blocking for " + key);
        close(_socket);
        freeaddrinfo(res);
        return;
    }
    if (setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, &sockOpt, sizeof(sockOpt)))
    {
        addConfigError(ERR "Failed to set socket options for " + key);
        close(_socket);
        freeaddrinfo(res);
        return;
    }
    if (res->ai_family == AF_INET6)
        setsockopt(_socket, IPPROTO_IPV6, IPV6_V6ONLY, &sockOpt, sizeof(sockOpt));
    if (bind(_socket, res->ai_addr, res->ai_addrlen))
    {
        addConfigError(ERR "Failed to bind " + key + " (" + strerror(errno) + ")");
        close(_socket);
        freeaddrinfo(res);
        return;
    }
    freeaddrinfo(res);
    if (listen(_socket, SOMAXCONN))
    {
        addConfigError(ERR "Failed to listen on " + key);
        close(_socket);
        return;
    }
    socketMap[key] = _socket;
    cout << LISTENING << key + "\n";
    ep.event.data.fd = _socket;
    ep.event.events = EPOLLIN;
    if (epoll_ctl(ep.epollFd, EPOLL_CTL_ADD, _socket, &ep.event))
    {
        addConfigError(ERR "Failed to add " + key + " to epoll");
        socketMap.erase(key);
        close(_socket);
        return;
    }
}
