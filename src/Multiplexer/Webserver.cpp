#include "../../inc/webserv.hpp"
#include "../../inc/Server.hpp"


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