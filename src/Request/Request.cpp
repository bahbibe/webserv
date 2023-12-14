#include "../../inc/Request.hpp"

Request::~Request()
{
}

Request::Request(int socket, epoll_event event) : _socketFd(socket), _event(event), _lineCount(0)
{
    memset(_buffer, 0, BUFFER_SIZE);
    this->readRequest();
    std::map<std::string, std::string>::iterator it = _headers.find("host");
    if (it == _headers.end())
        throw Server::ServerException(ERR "Invalid request (no host header)");
    std::cout << "Method: " << _method << std::endl;
    std::cout << "Request Target: " << _requestTarget << std::endl;
    std::cout << "HTTP Version: " << _httpVersion << std::endl;
    std::cout << "Headers size: " << _headers.size() << std::endl;
    std::cout << "Headers: " << std::endl;
    it = _headers.begin();
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
        _buffer[readBytes] = '\0';
        _request.append(_buffer, readBytes);
        this->parseRequest(_buffer);
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

typedef std::pair<std::map<std::string, std::string>::iterator, bool> ret_type;

void Request::parseRequest(std::string buffer)
{
    while (buffer.length() > 0)
    {
        if (this->_lineCount == 0)
            this->parseRequestLine(buffer);
        else
        {
            size_t pos = buffer.find("\r\n");
            if (pos == std::string::npos)
                break;
            std::string header = buffer.substr(0, pos);
            buffer.erase(0, pos + 2);
            if (header.length() != 0)
            {
                std::vector<std::string> headerTokens = this->split(header, ": ");
                std::string headerName = toLowerCase(headerTokens[0]);
                std::string headerValue = headerTokens[1];
                ret_type ret = this->_headers.insert(std::pair<std::string, std::string>(headerName, headerValue));
                if (headerName == "host" && ret.second == false)
                    throw Server::ServerException(ERR "Invalid request (duplicate host header)");
            }
        }
    }
    
}


void Request::parseRequestLine(std::string& buffer)
{
    size_t pos = buffer.find("\r\n");
    if (pos == std::string::npos)
        throw Server::ServerException(ERR "Invalid request (no \\r\\n)");
    std::string requestLine = buffer.substr(0, pos);
    buffer.erase(0, pos + 2);
    std::vector<std::string> tokens = this->split(requestLine, " ");
    if (tokens.size() != 3)
        throw Server::ServerException(ERR "Invalid request (size not 3)");
    this->_method = tokens[0];
    this->_requestTarget = tokens[1];
    this->_httpVersion = tokens[2];
    if (this->_method != "GET" && this->_method != "POST" && this->_method != "DELETE")
        throw Server::ServerException(ERR "Invalid request (invalid method)");
    // TODO: validate the request target
    if (this->_httpVersion != "HTTP/1.1")
        throw Server::ServerException(ERR "Invalid request (invalid http version)");
    this->_lineCount++;
}

void Request::validateRequest()
{
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