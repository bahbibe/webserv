#pragma once
#include "webserv.hpp"
#include "Server.hpp"
#include "Helpers.hpp"

#define BUFFER_SIZE 1024

class Request {
private:
    Server _server;
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

    string _fileFullPath;
    
    //? Parsing
    void parseRequest(string buffer);
    void parseRequestLine(string& requestLine);
    void parseBody(string buffer);
    void parseBodyWithContentLength(string buffer);
    void parseBodyWithChunked(string buffer);

    //? 
    void setStatusCode(int statusCode, string statusMessage);
    void createOutfile();

    //? Helpers
    vector<string> split(string str, string delimiter);
    void trim(string& str);
    string toLowerCase(const string &str);
    Location* findLocation() const;
public:
    bool isErrorCode;
    Request(Server &server);
    ~Request();
    Request() {}
    Request(Request const &other);
    Request &operator=(Request const &other);
    void readRequest(int socket);
    void validateRequest();

    void printRequest();

    //? Getters
    bool getIsRequestFinished() const;
    string getStatusMessage() const;
    string getMethod() const;
    string getRequestTarget() const;
    string getHttpVersion() const;
    int getStatusCode() const;
    map<string, string> getHeaders() const;
    fstream* getOutFile() const;
    string getFileFullPath() const;

    //? Setters
    void setContentLength(string contentLength);
};
