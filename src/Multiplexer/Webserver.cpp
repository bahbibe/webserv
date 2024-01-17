#include "../../inc/webserv.hpp"
#include "../../inc/Server.hpp"
#include "../../inc/Request.hpp"
#include "../../inc/Response.hpp"


Webserver::Webserver()
{
    if((ep.epollFd = epoll_create(1) ) == -1 )
        throw ServerException(ERR "Failed to create epoll");
}

size_t Webserver::serverCount()
{
    return _servers.size();
}

Server &Webserver::operator[](size_t index)
{
    return _servers[index];
}

Webserver::~Webserver()
{
    
}

void Webserver::brackets(string const &file)
{
    stringstream ss(file);
    string buff;
    stack<string> lim;
    while (ss >> buff)
    {
        if (buff == "server")
        {
            _servers.push_back(Server());
            ss >> buff;
            if (buff == OPEN_BR)
                lim.push(buff);
            else
                throw Server::ServerException(ERR "Expected '{'");
        }
        else if (buff == "location")
        {
            ss >> buff;
            if (buff[0] != '/')
                throw Server::ServerException(ERR "Expected '/'");
            ss >> buff;
            if (buff == OPEN_BR)
                lim.push(buff);
            else
                throw Server::ServerException(ERR "Expected '{'");
        }
        else if (buff == CLOSE_BR)
        {
            if (!lim.empty() && lim.top() == OPEN_BR)
                lim.pop();
            else
                throw Server::ServerException(ERR "Expected '}'");
        }
    }
    if (!lim.empty())
        throw Server::ServerException(ERR "Unclosed '{'");
}

void Webserver::newConnection(map<int, Request> &req, Server &server)
{
    int clientSock;
    struct sockaddr_in clientAddr;
    socklen_t addrLen = sizeof(clientAddr);
    if ((clientSock = accept(server.getSocket(), (struct sockaddr *)&clientAddr, &addrLen)) == -1)
        throw ServerException(ERR "Accept failed");
    cout << "New connection\n";
    ep.event.data.fd = clientSock;
    ep.event.events = EPOLLIN | EPOLLOUT | EPOLLHUP | EPOLLRDHUP | EPOLLERR;
    if (epoll_ctl(ep.epollFd, EPOLL_CTL_ADD, clientSock, &ep.event))
        throw ServerException(ERR "Failed to add client to epoll");
    req.insert(pair<int, Request>(clientSock, Request(&server, clientSock)));
}

bool Webserver::matchServer(map<int, Request> &req, int sock)
{
    for (size_t j = 0; j < _servers.size(); j++)
    {
        if (sock == _servers[j].getSocket())
        {
            newConnection(req, _servers[j]);
            return true;
        }
    }
    return false;
}

void Webserver::closeConnection(map<int, Request> &req, map<int, Response> &resp, int sock)
{
    cout << YELLOW "Connection closed\n" RESET;
    req.erase(sock);
    resp.erase(sock);
    close(sock);
}

void Webserver::start()
{
    signal(SIGPIPE, SIG_IGN);
    while (1)
    {
        int evCount = epoll_wait(ep.epollFd, ep.events, MAX_EVENTS, -1);
        for (int i = 0; i < evCount; i++)
        {
            if (matchServer(_req, ep.events[i].data.fd))
                continue;
            if (ep.events[i].events & EPOLLHUP || ep.events[i].events & EPOLLRDHUP || ep.events[i].events & EPOLLERR)
                closeConnection(_req, _resp, ep.events[i].data.fd);
            else
            {
                if (ep.events[i].events & EPOLLIN && !_req[ep.events[i].data.fd].getIsRequestFinished())
                {
                    _req[ep.events[i].data.fd].readRequest();
                    if (_req[ep.events[i].data.fd].getIsRequestFinished())
                        _resp.insert(pair<int, Response>(ep.events[i].data.fd, Response()));
                }
                if (ep.events[i].events & EPOLLOUT && _req[ep.events[i].data.fd].getIsRequestFinished())
                {
                    _resp[ep.events[i].data.fd].sendResponse(_req[ep.events[i].data.fd], ep.events[i].data.fd);
                    if (_resp[ep.events[i].data.fd].getIsFinished() == true)
                        closeConnection(_req, _resp, ep.events[i].data.fd);
                }
            }
        }
    }
}