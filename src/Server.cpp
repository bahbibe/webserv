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
    // if (access(buff.c_str(), F_OK) == -1)
    //     throw runtime_error(ERR "Error page doesn't exist");
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

void Server::start()
{
    int ep = epoll_create(1);
    epoll_event ev;
    epoll_event evs[1024];

    int sockfd;
    struct sockaddr_in serv_addr;
    int opt = 1;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
        throw runtime_error(ERR "Socket failed");
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
        throw runtime_error(ERR "Setsockopt failed");
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(_host.c_str());
    serv_addr.sin_port = htons(atoi(_port.c_str()));
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        throw runtime_error(ERR "Bind failed");
    if (listen(sockfd, 3) < 0)
        throw runtime_error(ERR "Listen failed");
    ev.data.fd = sockfd;
    ev.events = EPOLLIN;
    epoll_ctl(ep,EPOLL_CTL_ADD,sockfd,&ev);
    cout << GREEN "Server started on " RESET << _host << ":" << _port << "\n";
    while (1)
    {
        int new_socket;
        struct sockaddr_in address;
           

        int fd_ready = epoll_wait(ep,evs,1024,-1); 
        int addrlen = sizeof(address);
        for(int i = 0; i < fd_ready;i++)
        {
            if(evs[i].data.fd == sockfd)
            {
                if ((new_socket = accept(sockfd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
                    throw runtime_error(ERR "Accept failed");
                cout << "New connection\n";
                cout << new_socket << endl;
                ev.data.fd = new_socket;
                ev.events = EPOLLIN | EPOLLOUT;
                epoll_ctl(ep,EPOLL_CTL_ADD,new_socket,&ev);
            }
            if(evs[i].data.fd != sockfd  && evs[i].events & EPOLLIN)
            {
                cout << evs[i].data.fd << endl;
            }
            
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