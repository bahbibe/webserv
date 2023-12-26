#include "../inc/Server.hpp"
#include "../inc/Response.hpp"

Server::Server()
{
    _autoindex = false;
    memset(&_dir, 0, sizeof(_dir));
}

Server::~Server()
{
    map<string, Location *>::iterator it = _locations.begin();
    for (; it != _locations.end(); it++)
    {
        delete it->second;
    }
    close(_socket);
    close(_epoll);
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
    if ((_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        throw ServerException(ERR "Failed to create socket");
    if (setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR , &sockOpt, sizeof(sockOpt)))
        throw ServerException(ERR "Failed to set socket options");
    if (bind(_socket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)))
        throw ServerException(ERR "Failed to bind socket");
    if (listen(_socket, 1))
        throw ServerException(ERR "Failed to listen on socket");    
    cout << LISTENING << _host + ":" + _port + "\n";
}

void Server::setupEpoll()
{
    if((_epoll = epoll_create(1) ) == -1 )
        throw ServerException(ERR "Failed to create epoll");
    epoll_event event;
    event.data.fd = _socket;
    event.events = EPOLLIN;
    if (epoll_ctl(_epoll,EPOLL_CTL_ADD,_socket,&event))
        throw ServerException(ERR "Failed to add socket to epoll");
}
void Server::start()
{
    epoll_event evs[1024]; //!TODO reset evs after each epoll_wait
    setupSocket();
    setupEpoll();
    epoll_event ev;
    Request *req;
    Response resp;
    while (1)
    {
        int clientSock;
        struct sockaddr_in clientAddr;
        socklen_t addrLen = sizeof(clientAddr);
        int evCount = epoll_wait(_epoll,evs,MAX_EVENTS,-1); 
        for(int i = 0; i < evCount;i++)
        {
            // Request req;
            if(evs[i].data.fd == _socket)
            {
                if ((clientSock = accept(_socket, (struct sockaddr *)&clientAddr, &addrLen)) == -1)
                    throw ServerException(ERR "Accept failed");
                cout << "New connection\n";
                ev.data.fd = clientSock;
                ev.events = EPOLLIN | EPOLLOUT;
                req = new Request();
                epoll_ctl(_epoll,EPOLL_CTL_ADD,clientSock,&ev);
            }
            if(evs[i].data.fd != _socket  && evs[i].events & EPOLLIN)
            {
                req->readRequest(evs[i].data.fd);
                // if (req->getIsRequestFinished())
                //     cout << GREEN "Request finished\n" RESET;
                // try
                // {
                //     // TODO: support telnet for reading request
                //     req.readRequest(evs[i].data.fd);
                //     // req.validateRequest();
                // }
                // catch(int statusCode)
                // {
                //     std::cerr << req.getStatusMessage() << '\n';
                //     // exit(0);
                //     close(evs[i].data.fd);
                // }
            }
            if(evs[i].data.fd != _socket  && evs[i].events & EPOLLOUT && req->getIsRequestFinished())
            {
                resp.sendResponse(*req, evs[i].data.fd);
                // cout << "Request finished\n";
                // Response res(req);
                // res.sendResponse(evs[i].data.fd);
                // close(evs[i].data.fd);
            }
            // if (req && req->getIsRequestFinished())
            // {
            //     delete req;
            //     close(evs[i].data.fd);
            // }
        }
        
        
        // Request *req = new Request(new_socket, _locations, _error_pages, _server_root, _autoindex, _client_max_body_size);
        // req->parseRequest();
        // req->print();
        // Response *res = new Response(req);
        // res->sendResponse();
        // delete req;
        // delete res;
    }


}