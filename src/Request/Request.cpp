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
    // std::cout << "------ Request Start -----" << std::endl;
    // std::cout << _request << std::endl;
    // std::cout << "------ Request END -----" << std::endl;
    this->parseRequest();
    std::cout << "Method: " << _method << std::endl;
    std::cout << "Request Target: " << _requestTarget << std::endl;
    std::cout << "HTTP Version: " << _httpVersion << std::endl;
    std::cout << "Headers: " << std::endl;
    std::map<std::string, std::string>::iterator it = _headers.begin();
    for (; it != _headers.end(); it++)
        std::cout << it->first << ": " << it->second << std::endl;
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

std::string Request::toLowerCase(const std::string& str)
{
    std::string lowerCaseStr = "";
    for (size_t i = 0; i < str.length(); i++)
        lowerCaseStr += std::tolower(str[i]);
    return lowerCaseStr;
}

void Request::parseRequest()
{
    size_t pos = this->_request.find("\r\n");
    if (pos == std::string::npos)
        throw Server::ServerException(ERR "Invalid request");
    std::string requestLine = this->_request.substr(0, pos);
    this->_request.erase(0, pos + 2);
    std::vector<std::string> requestLineTokens = split(requestLine, " ");
    if (requestLineTokens.size() != 3)
        throw Server::ServerException(ERR "Invalid request");
    this->_method = requestLineTokens[0];
    this->_requestTarget = requestLineTokens[1];
    this->_httpVersion = requestLineTokens[2];
    pos = this->_request.find("\r\n");
    while (pos != std::string::npos)
    {
        std::string headerLine = this->_request.substr(0, pos);
        this->_request.erase(0, pos + 2);
        if (this->_request.empty())
            break;
        std::vector<std::string> headerLineTokens = split(headerLine, ": ");
        if (headerLineTokens.size() != 2)
            throw Server::ServerException(ERR "Invalid request");
        this->_headers.insert(std::pair<std::string, std::string>(this->toLowerCase(headerLineTokens[0]), headerLineTokens[1]));
        pos = this->_request.find("\r\n");
    }
    std::map<std::string, std::string>::iterator it = this->_headers.find("host");
    if (it == this->_headers.end())
        throw Server::ServerException(ERR "Invalid request");
    this->validateRequest();
}

void Request::validateRequest()
{
    // validate the method
    if (this->_method != "GET" && this->_method != "POST" && this->_method != "DELETE")
        throw Server::ServerException(ERR "Invalid request");
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