#pragma once
#include "webserv.hpp"
#include "Server.hpp"
#include "Helpers.hpp"

#define BUFFER_SIZE 1024

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
    string returnRedirect;
    string requestedFile;
};

class Request {
private:
    Server* _server;
    int _socketFd;
    int _lineCount;
    int _statusCode;
    string _statusMessage;
    bool _isRequestFinished;
    bool _isFoundCRLF;
    
    char _buffer[BUFFER_SIZE];
    string _request;

    string _method;
    string _requestTarget;
    string _httpVersion;
    map<string, string> _headers;
    string _filePath;
    fstream *_outfile;
    bool _outfileIsCreated;
    size_t _bodyLength;
    bool _isReadingBody;
    size_t _contentLength;

    string _fileFullPath;

    map<string, vector<string> > _mimeTypes;
    
    //? Parsing
    void parseRequest(string buffer);
    void parseRequestLine(string& requestLine);
    void parseBody(string buffer);
    void parseBodyWithContentLength(string buffer);
    void parseBodyWithChunked(string buffer);

    //? Request Helpers
    void setStatusCode(int statusCode, string statusMessage);
    void setContentLength(string contentLength);
    void createOutfile();
    void setServer();
    string getMimeType(string contentType);

    //? Helpers
    vector<string> split(string str, string delimiter);
    void trim(string& str);
    string toLowerCase(const string &str);
    Location* findLocation();
public:
    //? Server directives
    Directives directives;

    Location *_location;
    bool isErrorCode;
    Request(Server* server, int socketFd);
    ~Request();
    Request() {}
    Request(Request const &other);
    Request &operator=(Request const &other);
    void readRequest();
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
    Location* getLocation() const;
    string getFileFullPath() const;
};
