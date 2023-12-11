#include "../../inc/Request.hpp"

Request::~Request()
{
}

Request::Request(int socket, epoll_event event)
{
    _socketFd = socket;
    _event = event;
    memset(_buffer, 0, BUFFER_SIZE);
    this->readRequest();
    std::cout << "------ Request Start -----" << std::endl;
    std::cout << _request << std::endl;
    std::cout << "------ Request END -----" << std::endl;
    this->parseRequest();
    // std::cout << "Method: " << _method << std::endl;
    // std::cout << "Request Target: " << _requestTarget << std::endl;
    // std::cout << "HTTP Version: " << _httpVersion << std::endl;
    // std::cout << "Host: " << _host << std::endl;
}

void Request::readRequest()
{
    int readBytes = recv(_socketFd, _buffer, BUFFER_SIZE, 0);
    // int readBytes = read(_socketFd, _buffer, BUFFER_SIZE);
    if (readBytes == -1)
        throw Server::ServerException(ERR "Failed to read from socket");
    while (readBytes > 0)
    {
        _request.append(_buffer, readBytes);
        if (_request.find("\r\n\r\n") != std::string::npos)
            break;
        readBytes = recv(_socketFd, _buffer, BUFFER_SIZE, 0);
        // readBytes = read(_socketFd, _buffer, BUFFER_SIZE);
    }
}


std::vector<std::string> Request::split(std::string str, std::string delimiter)
{
    std::vector<std::string> tokens;
    size_t pos = str.find(delimiter);
    while (pos != std::string::npos)
    {
        tokens.push_back(str.substr(0, pos));
        str.erase(0, pos + delimiter.length());
        pos = str.find(delimiter);
    }
    tokens.push_back(str);
    return tokens;
}

void Request::parseRequest()
{
    size_t pos = _request.find("\r\n");
    if (pos == std::string::npos)
        throw Server::ServerException(ERR "Invalid request: empty request");
    std::string requestLine = _request.substr(0, pos);
    _request.erase(0, pos + 2);
    std::vector<std::string> requestLineTokens = this->split(requestLine, " ");
    if (requestLineTokens.size() != 3)
        throw Server::ServerException(ERR "Invalid request: invalid first line");
    _method = requestLineTokens[0];
    _requestTarget = requestLineTokens[1];
    _httpVersion = requestLineTokens[2];
    // if (_requestTarget[0] != '/')
    //     throw Server::ServerException(ERR "Invalid request");
    pos = _request.find("\r\n");
    size_t endPos = _request.find("\r\n\r\n");
    while (pos != std::string::npos && pos < endPos)
    {
        std::string header = _request.substr(0, pos);
        _request.erase(0, pos + 2);
        std::vector<std::string> headerTokens = this->split(header, ": ");
        std::cout << "header: "<< header << ", header size: " << headerTokens.size() << std::endl;
        if (headerTokens.size() != 2)
            throw Server::ServerException(ERR "Invalid request: invalid header");
        if (headerTokens[0] == "Host")
            _host = headerTokens[1];
        pos = _request.find("\r\n");
    }
    if (_host.empty())
        throw Server::ServerException(ERR "Invalid request: no host header");
}

void Request::runTests()
{
    // std::string test = "hello";
    // std::vector<std::string> testTokens = split(test, ": ");
    // std::cout << "Size: " << testTokens.size() << std::endl;
    // std::vector<std::string>::iterator it = testTokens.begin();
    // for (; it != testTokens.end(); it++)
    //     std::cout << *it << std::endl;
}