#pragma once

#include "webserv.hpp"
#include "Server.hpp"

#define BUFFER_SIZE 1024

class Request {
private:
    int _socketFd;
    epoll_event _event;
    int _lineCount;
    
    char _buffer[BUFFER_SIZE];
    std::string _request;

    // request stuff
    std::string _method;
    std::string _requestTarget;
    std::string _httpVersion;
    std::map<std::string, std::string> _headers;
    std::string _body;

    int _statusCode;
    
    void readRequest();
    void parseRequest(std::string buffer);
    void parseRequestLine(std::string& requestLine);
    void validateRequest();
    std::vector<std::string> split(std::string str, std::string delimiter);
    std::string toLowerCase(const std::string &str);
    void throwException(const std::string& msg, int statusCode);
public:
    Request(int socket, epoll_event event);
    ~Request();

    static void runTests();

};

/*
POST /post.php HTTP/1.1
Host: localhost:8080
Content-Type: application/x-www-form-urlencoded
Content-Length: 23

name=JohnWick&age=30

*/