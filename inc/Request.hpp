#pragma once

#include "webserv.hpp"
#include "Server.hpp"

#define BUFFER_SIZE 1024

class Request {
private:
    int _socketFd;
    epoll_event _event;
    int _requestLine;
    
    char _buffer[BUFFER_SIZE];
    std::string _request;

    // request stuff
    std::string _method;
    std::string _requestTarget;
    std::string _httpVersion;
    std::string _host;
    std::map<std::string, std::string> _headers;
    
    void readRequest();
    void parseRequest(std::string buffer);
    void parseRequestLine(std::string requestLine);
    void validateRequest();
    std::vector<std::string> split(std::string str, std::string delimiter);
    std::string toLowerCase(const std::string &str);
public:
    Request(int socket, epoll_event event);
    ~Request();


    static void runTests();

};