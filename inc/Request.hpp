#pragma once

#include "webserv.hpp"
#include "Server.hpp"
#include "Helpers.hpp"

#define BUFFER_SIZE 1024

class Request {
private:
    Server *_server;
    int _socketFd;
    int _lineCount;
    int _statusCode;
    string _statusMessage;
    bool isRequestFinished;
    bool _isFoundCRLF;
    
    char _buffer[BUFFER_SIZE];
    string _request;

    string _method;
    string _requestTarget;
    string _httpVersion;
    map<string, string> _headers;
    string _uploadFilePath;
    string _filePath;
    fstream *_outfile;
    bool _outfileIsCreated;
    size_t _bodyLength;
    bool _isReadingBody;
    size_t _contentLength;

    Location *_location;
    
    void parseRequest(string buffer);
    void parseRequestLine(string& requestLine);
    void parseBody(string buffer);
    void parseBodyWithContentLength(string buffer);
    void parseBodyWithChunked(string buffer);
    vector<string> split(string str, string delimiter);
    string toLowerCase(const string &str);
    void setStatusCode(int statusCode, string statusMessage);
    void createOutfile();

    void trim(string& str);
public:
    bool isErrorCode;
    Request(Server* server);
    ~Request();

    void readRequest(int socket);
    void validateRequest();

    void printRequest();

    bool getIsRequestFinished() const;
    string getStatusMessage() const;
    string getMethod() const;
    string getRequestTarget() const;
    string getHttpVersion() const;
    int getStatusCode() const;
    map<string, string> getHeaders() const;
    fstream* getOutFile() const;

    void setContentLength(string contentLength);
    Location* getLocation() const;
};

/*
POST /post.php HTTP/1.1
Host: localhost:8080
Content-Type: application/x-www-form-urlencoded
Content-Length: 25

name=JohnWick&age=30

*/