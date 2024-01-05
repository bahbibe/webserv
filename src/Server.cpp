#include "../inc/Server.hpp"
#include "../inc/Response.hpp"

streampos Server::_pos = 0;
Server::Server() : _autoindex(false)
{
    memset(&_dir, 0, sizeof(_dir));
}

Server::~Server()
{
    map<string, Location *>::iterator it = _locations.begin();
    for (; it != _locations.end(); it++)
    {
        // if(it->second)
        //     delete it->second;
    }
    close(_socket);
}

int Server::getSocket() const
{
    return _socket;
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
    throw ServerException(ERR "Invalid error code");
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

void Server::setupSocket()
{
    int sockOpt = 1;
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(_host.c_str());
    serverAddr.sin_port = htons(atoi(_port.c_str()));
    if ((_socket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) == -1)
        throw ServerException(ERR "Failed to create socket");
    if (setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR , &sockOpt, sizeof(sockOpt)))
        throw ServerException(ERR "Failed to set socket options");
    if (bind(_socket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)))
        throw ServerException(ERR "Failed to bind socket");
    if (listen(_socket, 1))
        throw ServerException(ERR "Failed to listen on socket");    
    cout << LISTENING << _host + ":" + _port + "\n";
    ep.event.data.fd = _socket;
    ep.event.events = EPOLLIN;
    if (epoll_ctl(ep.epollFd,EPOLL_CTL_ADD,_socket,&ep.event))
        throw ServerException(ERR "Failed to add socket to epoll");
}


void Webserver::newConnection(map<int, Request> &req ,Server &server)
{
    int clientSock;
    struct sockaddr_in clientAddr;
    socklen_t addrLen = sizeof(clientAddr);
    if ((clientSock = accept(server.getSocket(), (struct sockaddr *)&clientAddr, &addrLen)) == -1)
        throw ServerException(ERR "Accept failed");
    cout << "New connection\n";
    struct epoll_event ev;
    ev.data.fd = clientSock;
    ev.events = EPOLLIN | EPOLLOUT;
    if (epoll_ctl(ep.epollFd,EPOLL_CTL_ADD,clientSock,&ev))
        throw ServerException(ERR "Failed to add client to epoll");
    req.insert(pair<int, Request>(clientSock, Request(&server, clientSock)));
}

void Webserver::start()
{
    signal(SIGPIPE, SIG_IGN);
    map<int, Request> req;
    map<int, Response> resp;
    while (1)
    {
        int evCount = epoll_wait(ep.epollFd, ep.events, MAX_EVENTS, -1);
        for(int i = 0; i < evCount;i++)
        {
            for (size_t j = 0; j < _servers.size(); j++)
            {
                if(ep.events[i].data.fd == _servers[j].getSocket())
                {
                    newConnection(req, _servers[j]);
                    continue;
                }
                if(ep.events[i].events & EPOLLIN)
                {
                    req[ep.events[i].data.fd].readRequest();
                    if(req[ep.events[i].data.fd].getIsRequestFinished())
                    {
                        resp.insert(pair<int, Response>(ep.events[i].data.fd, Response()));
                    }
                }
                if(ep.events[i].events & EPOLLOUT && req[ep.events[i].data.fd].getIsRequestFinished())
                {
                    resp[ep.events[i].data.fd].sendResponse(req[ep.events[i].data.fd], ep.events[i].data.fd);
                    if (resp[ep.events[i].data.fd].getIsFinished() == true)
                    {
                        req.erase(ep.events[i].data.fd);
                        resp.erase(ep.events[i].data.fd);
                        close(ep.events[i].data.fd);
                    }

                }
            }
        }
    }
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

map<string, Location *> Server::getLocations() const
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