#pragma once

#include "webserv.hpp"
#include "Server.hpp"

#define BUFFER_SIZE 1024

class Request {
private:
    char _buffer[BUFFER_SIZE];
    int _socketFd;
    std::string _request;
    epoll_event _event;

    // request stuff
    std::string _method;
    std::string _requestTarget;
    std::string _httpVersion;
    std::string _host;
    
    void readRequest();
    void parseRequest();
    std::vector<std::string> split(std::string str, std::string delimiter);
public:
    Request(int socket, epoll_event event);
    ~Request();


    static void runTests();

};