#pragma once
#include "webserv.hpp"
#include "Server.hpp"
#include "Helpers.hpp"
#include "Boundaries.hpp"
#include "Chunks.hpp"
#include <climits>
#include <sys/time.h>

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
    map<string, string> cgiPaths;
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
    bool _isRequestFinished;
    bool _isFoundCRLF;
    
    char _buffer[BUFFER_SIZE];
    string _requestBuffer;
    string _headersBuffer;
    string _rest;

    string _method;
    string _requestTarget;
    string _tmpRequestTarget;
    string _httpVersion;
    map<string, string> _headers;
    string _filePath;
    fstream _outfile;
    bool _outfileIsCreated;
    size_t _bodyLength;
    bool _isReadingBody;
    size_t _contentLength;
    string _host;

    bool _isBodyBoundary;
    string _boundary;
    bool _wantsClose;

    // POST keep-alive support: a POST whose body length is known up
    // front (Content-Length or multipart, never chunked - see
    // captureDeclaredBodyLength()) can reuse its connection once its
    // full declared body has actually been read off the wire, whether
    // that happens via normal success or via an early error. Draining
    // covers the gap when an error fires before the client has
    // finished sending: the rest of the declared body still has to be
    // read and discarded before the socket is safe to hand to a new
    // Request object.
    bool _hasContentLength;
    size_t _declaredContentLength;
    size_t _bodyBytesConsumed;
    bool _isDraining;
    size_t _drainRemaining;
    bool _drainTimedOut;

    // Chunked POST keep-alive support: a chunked body's length isn't
    // known up front, so it can't use the byte-counter drain above.
    // Instead, an error mid-body switches the same Chunks parser this
    // request already uses into discard mode (see
    // Chunks::discardFromNowOn()) and keeps feeding it bytes - it
    // already knows how to walk chunk framing correctly, including
    // the terminating "0" chunk and trailer part, so reusing it here
    // finds the true end of the client's declared body on the wire
    // the same way a normal successful parse would.
    // _chunkedBodyConsumed is the single source of truth for "the
    // chunks parser genuinely reached its real terminator" - set from
    // the same place (a caught 201 out of Chunks::parse()) whether
    // that happened during normal parsing or during a later drain.
    bool _isChunkedDraining;
    bool _chunkedBodyConsumed;
    void drainChunkedBody();

    Boundaries _boundaries;
    Chunks _chunks;
    
    bool _isCgi;

    map<string, vector<string> > _mimeTypes;
    Location _defaultLocation;

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
    void captureDeclaredBodyLength();
public:
    int bufferSize;
    //? Server directives
    Directives directives;
    Location *_location;
    bool isErrorCode;
    time_t _start;
    struct timeval _startTv;
    string _clientIp;
    bool _ready;
    vector<Server> servers;

    Request(Server* server, int socketFd, vector<Server> servers);
    ~Request();
    Request();
    Request(Request const &other);
    Request &operator=(Request const &other);
    void readRequest();
    void validateRequest();
    void setStatusCode(int statusCode, string statusMessage);

    //? Getters
    bool getIsRequestFinished() const;
    string getMethod() const;
    string getRequestTarget() const;
    string getHttpVersion() const;
    int getStatusCode() const;
    map<string, string> getHeaders() const;
    void setTimeout();
    bool getWantsClose() const;
    Server* getServer() const;
    bool isDraining() const;
    bool isDrainTimedOut() const;
    bool hasKnownBodyLength() const;
    // True once any chunked-body draining this connection needed has
    // concluded (isDraining() is false by construction whenever this
    // is meaningfully checked) without ever reaching a real
    // terminator - a malformed chunk mid-drain, not just "no chunked
    // body was involved at all". Checked at the same reuse-decision
    // site as isDrainTimedOut(), alongside it.
    bool chunkedBodyFailedToDrain() const;
    void abortDraining();
};
