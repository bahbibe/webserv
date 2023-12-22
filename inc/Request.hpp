#pragma once

#include "webserv.hpp"
#include "Server.hpp"

#define BUFFER_SIZE 1024

class Request {
private:
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
    Request();
    ~Request();

    void readRequest(int socket);
    void validateRequest();

    void printRequest();

    bool getIsRequestFinished() const;
    string getStatusMessage() const;
    string getMethod() const;
    string getRequestTarget() const;
    string getHttpVersion() const;
    map<string, string> getHeaders() const;
    fstream* getOutFile() const;
};

/*
POST /post.php HTTP/1.1
Host: localhost:8080
Content-Type: application/x-www-form-urlencoded
Content-Length: 25

name=JohnWick&age=30

*/