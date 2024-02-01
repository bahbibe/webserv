#pragma once
#include "webserv.hpp"
#include "Server.hpp"
#include "Helpers.hpp"
#include "Boundaries.hpp"
#include "Chunks.hpp"
#include <climits>

struct Directives {
    string host;
    string port;
    string serverRoot;
    size_t clientMaxBodySize;
    bool autoindex;
    map<string, string> errorPages;
    vector<string> indexs;
    vector<string> serverNames;
    bool isUploadAllowed;
    string uploadPath;
    bool isCgiAllowed;
    string cgiUploadPath;
    string returnRedirect;
    string requestTarget;
    string requestedFile;
    string queryString;
    string httpCookie;
    string httpAccept;
    string cgiFileName;
    string contentType;
    string boundary;
    size_t contentLength;
    map<string, string> types;
    bool isCGI;
};

class Request {
private:
    Server* _server;
    int _readBytes;
    int _socketFd;
    int _lineCount;
    int _statusCode;
    string _statusMessage;
    bool _isRequestFinished;
    bool _isFoundCRLF;
    
    char _buffer[BUFFER_SIZE];
    string _requestBuffer;
    string _headersBuffer;
    string _rest;

    string _method;
    string _requestTarget;
    string _httpVersion;
    map<string, string> _headers;
    string _filePath;
    fstream _outfile;
    bool _outfileIsCreated;
    size_t _bodyLength;
    bool _isReadingBody;
    size_t _contentLength;

    bool _isBodyBoundary;
    string _boundary;

    Boundaries _boundaries;
    Chunks _chunks;
    
    bool _isCgi;

    map<string, vector<string> > _mimeTypes;
    
    //? Parsing
    void parseRequest();
    void parseRequestLine();
    void parseHeaders();
    void parseBody();
    void parseBodyWithContentLength();
    void parseBodyWithChunked();
    void parseBodyWithBoundaries();

    //? Request Helpers
    void setContentLength(string contentLength);
    void createOutfile();
    void setServer();
    void validatePath();
    string getExtension(string contentType);
    void setDefaultDirectives();

    //? Helpers
    vector<string> split(string str, string delimiter);
    void trim(string& str);
    string toLowerCase(const string &str);
    void findServer();
    Location* findLocation();
public:
    int bufferSize;
    //? Server directives
    Directives directives;
    Location *_location;
    bool isErrorCode;
    clock_t _start;
    bool _ready;

    Request(Server* server, int socketFd);
    ~Request();
    Request();
    Request(Request const &other);
    Request &operator=(Request const &other);
    void readRequest();
    void validateRequest();
    void setStatusCode(int statusCode, string statusMessage);
    void printRequest();

    //? Getters
    bool getIsRequestFinished() const;
    string getStatusMessage() const;
    string getMethod() const;
    string getRequestTarget() const;
    string getHttpVersion() const;
    int getStatusCode() const;
    map<string, string> getHeaders() const;
    Location* getLocation() const;
    void setTimeout();
};
