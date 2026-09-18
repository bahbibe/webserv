#include "../../inc/Request.hpp"

typedef pair<map<string, string>::iterator, bool> ret_type;

Request::~Request()
{
    if (this->_outfileIsCreated)
        this->_outfile.close();
}
Request::Request(const Request &other)
{
    *this = other;
}

Request &Request::operator=(const Request &other)
{
    if (this != &other)
    {
        this->_readBytes = other._readBytes;
        this->_method = other._method;
        this->_requestTarget = other._requestTarget;
        this->_httpVersion = other._httpVersion;
        this->_headers = other._headers;
        this->_filePath = other._filePath;
        this->_socketFd = other._socketFd;
        this->_server = other._server;
        this->_defaultLocation = other._defaultLocation;
        this->_location = (other._location == &other._defaultLocation) ? &this->_defaultLocation : other._location;
        this->_lineCount = other._lineCount;
        this->_statusCode = other._statusCode;
        this->_isRequestFinished = other._isRequestFinished;
        this->_isFoundCRLF = other._isFoundCRLF;
        this->_outfileIsCreated = other._outfileIsCreated;
        this->_bodyLength = other._bodyLength;
        this->_isReadingBody = other._isReadingBody;
        this->_contentLength = other._contentLength;
        this->isErrorCode = other.isErrorCode;
        this->_isBodyBoundary = other._isBodyBoundary;
        this->_boundary = other._boundary;
        this->_wantsClose = other._wantsClose;
        this->_hasContentLength = other._hasContentLength;
        this->_declaredContentLength = other._declaredContentLength;
        this->_bodyBytesConsumed = other._bodyBytesConsumed;
        this->_isDraining = other._isDraining;
        this->_drainRemaining = other._drainRemaining;
        this->_drainTimedOut = other._drainTimedOut;
        this->_isChunkedDraining = other._isChunkedDraining;
        this->_chunkedBodyConsumed = other._chunkedBodyConsumed;

        this->_headersBuffer = other._headersBuffer;
        this->_requestBuffer = other._requestBuffer;
        this->_rest = other._rest;
        this->_tmpRequestTarget = other._tmpRequestTarget;
        this->_host = other._host;

        this->_boundaries = other._boundaries;
        this->_chunks = other._chunks;

        this->bufferSize = other.bufferSize;

        this->_isCgi = other._isCgi;
        this->servers = other.servers;
        this->directives = other.directives;
        this->_ready = other._ready;
    }
    return *this;
}

Request::Request() : _socketFd(0), _lineCount(0), _statusCode(200), _isRequestFinished(false),
    _isFoundCRLF(false),  _outfileIsCreated(false), _bodyLength(0),
    _isReadingBody(false), _contentLength(0), _isBodyBoundary(false), _wantsClose(false),
    _hasContentLength(false), _declaredContentLength(0), _bodyBytesConsumed(0),
    _isDraining(false), _drainRemaining(0), _drainTimedOut(false),
    _isChunkedDraining(false), _chunkedBodyConsumed(false),
    _isCgi(false), isErrorCode(false) , _ready(false)
{
    this->_readBytes = 0;
    this->_location = NULL;
    memset(_buffer, 0, BUFFER_SIZE + 1);
    this->bufferSize = BUFFER_SIZE;
    this->_start = 0;
    memset(&_startTv, 0, sizeof(_startTv));
}

Request::Request(Server* server, int socketFd, vector<Server> servers) : _socketFd(socketFd), _lineCount(0), _statusCode(200), _isRequestFinished(false),
    _isFoundCRLF(false),  _outfileIsCreated(false), _bodyLength(0),
    _isReadingBody(false), _contentLength(0), _isBodyBoundary(false), _wantsClose(false),
    _hasContentLength(false), _declaredContentLength(0), _bodyBytesConsumed(0),
    _isDraining(false), _drainRemaining(0), _drainTimedOut(false),
    _isChunkedDraining(false), _chunkedBodyConsumed(false),
    _isCgi(false), isErrorCode(false), _ready(false)
{
    this->servers = servers;
    this->_server = server;
    this->_readBytes = 0;
    this->_location = NULL;
    memset(_buffer, 0, BUFFER_SIZE + 1);
    this->bufferSize = BUFFER_SIZE;
    this->_start = 0;
    memset(&_startTv, 0, sizeof(_startTv));
}

void Request::readRequest()
{
    try {
        _start = time(NULL);
        if (this->_isChunkedDraining)
        {
            drainChunkedBody();
            return;
        }
        if (this->_isDraining)
        {
            char drainBuf[BUFFER_SIZE];
            size_t want = min((size_t)BUFFER_SIZE, this->_drainRemaining);
            ssize_t n = tlsAwareRead(_socketFd, drainBuf, want);
            if (n <= 0)
                return;
            this->_drainRemaining -= (size_t)n;
            if (this->_drainRemaining == 0)
                this->_isDraining = false;
            return;
        }
        _requestBuffer.clear();
        _readBytes = tlsAwareRead(_socketFd, _buffer, bufferSize);
        if (_readBytes <= 0)
            return;
        _buffer[_readBytes] = '\0';
        this->parseRequest();
    } catch (int statusCode)
    {
        return;
    }
}

void Request::parseRequest()
{
    _requestBuffer.append(_buffer, _readBytes);
    if (_isReadingBody)
        return this->parseBody();
    _headersBuffer.append(_requestBuffer);
    if (_headersBuffer.length() > MAX_HEADER_BYTES)
    {
        _wantsClose = true;
        setStatusCode(400, "Header block too large");
    }
    size_t pos = _headersBuffer.find("\r\n\r\n");
    if (pos == string::npos)
        return;
    _ready = true;
    _headersBuffer = _headersBuffer.substr(0, pos);
    _readBytes -= _headersBuffer.length() + 4;
    _requestBuffer.erase(0, _headersBuffer.length() + 4);
    parseRequestLine();
    parseHeaders();
    captureDeclaredBodyLength();
    _headersBuffer.clear();
    parseBody();
}

// Read-only extraction, run once headers are parsed and before any
// validation - just so an error raised anywhere downstream (even one
// that fires before setContentLength() would otherwise run, like a
// 405 from setServer()) already knows whether the declared body
// length is known and how large it is. A malformed Content-Length
// here is left alone - the real validation and its 400 still happen
// in setContentLength() when the POST/multipart path actually runs.
void Request::captureDeclaredBodyLength()
{
    if (_headers.find("transfer-encoding") != _headers.end())
        return;
    map<string, string>::iterator it = _headers.find("content-length");
    if (it == _headers.end() || it->second.empty())
        return;
    for (size_t i = 0; i < it->second.length(); i++)
        if (!isdigit(static_cast<unsigned char>(it->second[i])))
            return;
    stringstream ss(it->second);
    ss >> _declaredContentLength;
    _hasContentLength = true;
}

void Request::parseRequestLine()
{
    string requestLine = _headersBuffer.substr(0, _headersBuffer.find("\r\n"));
    _headersBuffer.erase(0, _headersBuffer.find("\r\n") + 2);
    vector<string> tokens = this->split(requestLine, " ");
    if (tokens.size() != 3)
        this->setStatusCode(400, "Invalid Request Line");
    this->_method = tokens[0];
    this->_requestTarget = tokens[1];
    this->_httpVersion = tokens[2];
    if (this->_method != "GET" && this->_method != "POST" && this->_method != "DELETE" && this->_method != "HEAD")
        setStatusCode(501, "Invalid Method");
    if (this->_requestTarget.empty() || !Helpers::checkURICharSet(this->_requestTarget))
        setStatusCode(400, "Invalid Request Target");
    if (this->_requestTarget.length() > 1024)
        setStatusCode(414, "Request-URI Too Long");
    if (!Helpers::decodeURI(_requestTarget))
        setStatusCode(400, "Invalid Request Target");
    if (this->_httpVersion.length() != 8 || this->_httpVersion.substr(0, 5) != "HTTP/")
        setStatusCode(400, "Invalid HTTP Version");
    else if (this->_httpVersion.substr(5, 3) != "1.1")
        setStatusCode(505, "HTTP Version Not Supported");
}

void Request::parseHeaders()
{
    while (_headersBuffer.length() > 0)
    {
        string header = _headersBuffer.substr(0, _headersBuffer.find("\r\n"));
        _headersBuffer.erase(0, header.length() + 2);
        vector<string> headerTokens = this->split(header, ": ");
        string headerName = toLowerCase(headerTokens[0]);
        string headerValue = headerTokens.size() > 1 ? headerTokens[1] : "";
        trim(headerValue);
        if (headerName == "cookie")
            directives.httpCookie = headerValue;
        if (headerName == "accept")
            directives.httpAccept = headerValue;
        if (headerName == "host")
            _host = headerValue;
        if (headerName == "connection")
            _wantsClose = (toLowerCase(headerValue) == "close");
        ret_type ret = this->_headers.insert(pair<string, string>(headerName, headerValue));
        if (ret.second == false)
            setStatusCode(400, "Duplicate Header");
    }
}

void Request::setContentLength(string contentLength)
{
    for (size_t i = 0; i < contentLength.length(); i++)
        if (!isdigit(contentLength[i]))
            setStatusCode(400, "Invalid Content-Length");
    stringstream ss(contentLength);
    ss >> this->_contentLength;
    directives.contentLength = this->_contentLength;
}

Location* Request::findLocation()
{
    map<string, unique_ptr<Location> > const &locations = this->_server->getLocations();
    map<string, unique_ptr<Location> >::const_iterator itb = locations.begin();
    map<string, unique_ptr<Location> >::const_iterator ite = locations.end();
    while (locations.size() > 0 && ite-- != itb)
    {
        size_t len = ite->first.length();
        // A raw prefix compare would match "/admin" against a target
        // of "/administrator" - same class of bug as validatePath()'s
        // sibling-directory bypass, just at the location-matching
        // layer instead of the filesystem layer. Require either an
        // exact match, or a '/' boundary right where the location
        // path ends (already true for "/" itself, since it ends in
        // '/') - "/admin" matches "/admin" and "/admin/x", not
        // "/administrator".
        bool exact = this->_requestTarget.length() == len
            && !this->_requestTarget.compare(0, len, ite->first);
        bool prefixWithBoundary = this->_requestTarget.length() > len
            && !this->_requestTarget.compare(0, len, ite->first)
            && (ite->first[len - 1] == '/' || this->_requestTarget[len] == '/');
        if (exact || prefixWithBoundary)
        {
            this->_requestTarget.erase(0, len);
            return ite->second.get();
        }
    }
    return NULL;
}

void Request::setDefaultDirectives()
{
    directives.host = _server->getHost();
    directives.port = _server->getPort();
    directives.serverRoot = _server->getRoot();
    directives.autoindex = _server->getAutoindex();
    directives.clientMaxBodySize = _server->getClientMaxBodySize();
    directives.errorPages = _server->getErrorPages();
    directives.indexs = _server->getIndexs();
    directives.serverNames = _server->getServerNames();
    directives.isUploadAllowed = false;
    directives.uploadPath = "";
    directives.isCgiAllowed = false;
    directives.contentLength = 0;
}

void Request::findServer()
{
    string hostName = _server->getHost() + ":" + _server->getPort();
    vector<Server>::iterator itb = servers.begin();
    for (; itb != servers.end(); itb++)
    {
        if (hostName == itb->getHost() + ":" + itb->getPort())
        {
            vector<string> serverNames = itb->getServerNames();
            vector<string>::iterator it = serverNames.begin();
            for (; it != serverNames.end(); it++)
            {
                if (*it == _host)
                {
                    _server = &(*itb);
                    return;
                }
            }
        }
    }
}

void Request::setServer()
{
    if (this->_requestTarget.find("?") != string::npos)
    {
        directives.queryString = this->_requestTarget.substr(this->_requestTarget.find("?") + 1);
        directives.requestTarget = this->_requestTarget.substr(0, this->_requestTarget.find("?"));
        _requestTarget = directives.requestTarget;
    }
    else
        directives.requestTarget = this->_requestTarget;
    _tmpRequestTarget = _requestTarget;
    findServer();
    this->_mimeTypes = _server->getExtensions();
    directives.types = _server->getTypes();
    setDefaultDirectives();
    _location = this->findLocation();
    if (_location == NULL)
    {
        _defaultLocation.setMethods("GET");
        _defaultLocation.setRoot(_server->getRoot());
        _defaultLocation.setClientMaxBodySize(_server->getClientMaxBodySize());
        _location = &_defaultLocation;
    }
    vector<string> locationMethods = _location->getMethods();
    if (locationMethods.size() > 0)
    {
        vector<string>::iterator itb = locationMethods.begin();
        vector<string>::iterator ite = locationMethods.end();
        string methodToCheck = (_method == "HEAD") ? "GET" : _method;
        if (find(itb, ite, methodToCheck) == ite)
            setStatusCode(405, "Method Not Allowed");
    }
    directives.isUploadAllowed = _location->getUpload();
    directives.uploadPath = _location->getUploadPath();
    directives.isCgiAllowed = _location->getCgi();
    directives.cgiUploadPath = _location->getCgiUploadPath();
    directives.cgiPaths = _location->getCgiPaths();
    directives.returnRedirect = _location->getReturn();
    directives.autoindex = _location->getAutoindex();
    directives.serverRoot = _location->getRoot();
    directives.indexs = _location->getIndexs();
    directives.clientMaxBodySize = _location->getClientMaxBodySize();
    if (directives.indexs.empty())
        directives.indexs = _server->getIndexs();
    if (!directives.serverRoot.empty() && directives.serverRoot[directives.serverRoot.length() - 1] != '/')
        directives.serverRoot += "/";
    if (!directives.uploadPath.empty() && directives.uploadPath[directives.uploadPath.length() - 1] != '/')
        directives.uploadPath += "/";
    if (!directives.cgiUploadPath.empty() && directives.cgiUploadPath[directives.cgiUploadPath.length() - 1] != '/')
        directives.cgiUploadPath += "/";
    directives.requestedFile = directives.serverRoot + this->_requestTarget;
    if (!directives.returnRedirect.empty())
        setStatusCode(301, "Moved Permanently");
}

void Request::validatePath()
{
    char realPath[PATH_MAX];
    char realRoot[PATH_MAX];
    if (realpath(directives.requestedFile.c_str(), realPath) != NULL
        && realpath(directives.serverRoot.c_str(), realRoot) != NULL)
    {
        string realPathStr = realPath;
        string realRootStr = realRoot;
        // A raw prefix compare here would treat a sibling directory
        // that merely shares root's name as a prefix - "/var/www/
        // site-secret" against a root of "/var/www/site" - as being
        // inside root, since the string "/var/www/site-secret"
        // starts with the string "/var/www/site". Requiring either an
        // exact match or a '/' right after root's length closes that:
        // realPathStr has to actually be root, or a real path
        // beneath it, not just share a character prefix with it.
        bool isInsideRoot = (realPathStr == realRootStr)
            || (realPathStr.length() > realRootStr.length()
                && realPathStr.compare(0, realRootStr.length(), realRootStr) == 0
                && realPathStr[realRootStr.length()] == '/');
        if (!isInsideRoot)
            setStatusCode(403, "Forbidden");
    }
}

void Request::validateRequest()
{
    this->setServer();
    this->validatePath();
    map<string, string>::iterator it = _headers.find("host");
    if (it == _headers.end() || it->second.length() == 0)
        setStatusCode(400, "No Host Header");
    directives.contentType = _headers["content-type"];
    if (_headers.find("content-type") != _headers.end() && _headers["content-type"].find("multipart/form-data") != string::npos)
    {
        if (_headers.find("transfer-encoding") != _headers.end())
            setStatusCode(501, "multipart/form-data and Transfer-Encoding are present");
        _isBodyBoundary = true;
        _boundary = "--" + _headers["content-type"].substr(_headers["content-type"].find("boundary=") + 9);
        setContentLength(_headers["content-length"]);
        _boundaries.setMimeTypes(_mimeTypes);
        _boundaries.setBoundaries(_boundary, directives.uploadPath, _contentLength);
        directives.boundary = _boundary;
    }
    if (_headers.find("transfer-encoding") != _headers.end() && _headers["transfer-encoding"] != "chunked")
        setStatusCode(501, "Unsupported Transfer-Encoding");
    if (_headers.find("content-length") != _headers.end() && _headers.find("transfer-encoding") != _headers.end())
        setStatusCode(400, "Both Content-Length and Transfer-Encoding are present");
    if (_method == "POST")
    {
        if (_headers.find("content-length") == _headers.end() && _headers.find("transfer-encoding") == _headers.end())
            setStatusCode(411, "Length Required");
        setContentLength(_headers["content-length"]);
        if (_headers.find("content-length") != _headers.end() && directives.clientMaxBodySize > 0 && this->_contentLength > directives.clientMaxBodySize)
            setStatusCode(413, "Request Entity Too Large");
    }
}

string Request::getExtension(string contentType)
{
    if (contentType.find(";") != string::npos)
        contentType = contentType.substr(0, contentType.find(";"));
    map<string, vector<string> >::iterator it = _mimeTypes.find(contentType);
    if (it != this->_mimeTypes.end())
    {
        if (it->second.size() > 0)
            return "." + it->second[0];
    }
    return ".bin";
}

void Request::createOutfile()
{
    directives.isCGI = false;
    if (directives.isCgiAllowed && Helpers::isCGI(directives.requestedFile, directives.indexs))
    {
        this->_isCgi = directives.isCGI = true;
        string randomFileName = Helpers::generateFileName();
        this->_filePath = directives.cgiUploadPath + randomFileName + ".cgi";
        directives.cgiFileName = this->_filePath;
        this->_outfile.open(this->_filePath.c_str(), ios::out | ios::binary);
        if (!this->_outfile.is_open())
            setStatusCode(500, "Failed to create file");
        this->_outfileIsCreated = true;
        return;
    }
    if (!directives.isUploadAllowed)
        setStatusCode(403, "upload is not allowed");
    if (_isBodyBoundary)
        return;
    string contentType = this->_headers["content-type"];
    string extension = this->getExtension(contentType);
    string randomFileName = Helpers::generateFileName();
    this->_filePath = directives.uploadPath + randomFileName + extension;
    this->_outfile.open(this->_filePath.c_str(), ios::out | ios::binary);
    if (!this->_outfile.is_open())
        setStatusCode(500, "Failed to create file");
    this->_outfileIsCreated = true;
    if (_headers.find("transfer-encoding") != _headers.end() && _headers["transfer-encoding"] == "chunked")
        this->_chunks.setChunks(&_outfile, _filePath, directives.clientMaxBodySize);
}

void Request::parseBodyWithBoundaries()
{
    try {
        _boundaries.parseBoundary(_requestBuffer, _readBytes);
    } catch (int statusCode)
    {
        setStatusCode(statusCode, "Boundaris Status Code");
    }
}

void Request::parseBody()
{
    this->_bodyBytesConsumed += (size_t)this->_readBytes;
    if (!this->_isReadingBody)
        this->validateRequest();
    if (this->_method != "POST")
        setStatusCode(200, "OK");
    this->_isReadingBody = true;
    if (!this->_outfileIsCreated)
        this->createOutfile();
    if (_isBodyBoundary && !_isCgi)
        parseBodyWithBoundaries();
    else if (_headers.find("transfer-encoding") != _headers.end() && _headers["transfer-encoding"] == "chunked")
        parseBodyWithChunked();
    else
        parseBodyWithContentLength();
}

void Request::parseBodyWithContentLength()
{
    if (_contentLength == 0)
        setStatusCode(201, "Created");
    if (_contentLength >= _requestBuffer.length())
    {
        this->_outfile.write(_requestBuffer.c_str(), _requestBuffer.length());
        this->_outfile.flush();
        _contentLength -= _requestBuffer.length();
    } else {
        this->_outfile.write(_requestBuffer.c_str(), _contentLength);
        this->_outfile.flush();
        _contentLength = 0;
    }
    if (_contentLength == 0)
        setStatusCode(201, "Created");
}

void Request::parseBodyWithChunked()
{
    try {
       bufferSize = _chunks.parse(_requestBuffer, _readBytes);
    } catch (int statusCode)
    {
        // 201 out of the chunks parser now only ever means it walked
        // all the way to the real terminator (the trailer part's
        // closing CRLF - see Chunks::parseTrailer()), not just "saw
        // the zero-size chunk announcement" - this is the single
        // place that fact gets recorded for the keep-alive decision.
        if (statusCode == 201)
            this->_chunkedBodyConsumed = true;
        setStatusCode(statusCode, "Chunks Status Code");
    }
}

// Driven by _isChunkedDraining, set from setStatusCode() below when
// an error fires before the client's declared chunked body has been
// fully read. Reuses this same request's _chunks parser (already
// mid-parse, or untouched and ready from its default-constructed
// state if the error fired before any body byte arrived at all) in
// discard mode - it already knows how to walk chunk framing
// correctly, so this just keeps feeding it bytes without writing any
// of them anywhere, until it reaches the real terminator or a
// framing error of its own.
void Request::drainChunkedBody()
{
    char drainBuf[BUFFER_SIZE];
    ssize_t n = tlsAwareRead(_socketFd, drainBuf, sizeof(drainBuf));
    if (n <= 0)
        return;
    try {
        _chunks.parse(string(drainBuf, (size_t)n), (int)n);
    } catch (int statusCode)
    {
        if (statusCode == 201)
            this->_chunkedBodyConsumed = true;
        this->_isChunkedDraining = false;
    }
}

void Request::setStatusCode(int statusCode, string statusMessage)
{
    this->_statusCode = statusCode;
    this->_isRequestFinished = true;
    if (statusCode >= 400)
        this->isErrorCode = true;
    // Every POST finishes here, success or error. If the client
    // declared a body length up front (Content-Length or multipart)
    // and hasn't actually sent all of it yet, the rest is still
    // coming on the wire and has to be read and discarded before this
    // connection can be handed to a new Request - otherwise those
    // leftover bytes get misparsed as the start of the next request.
    if (this->_method == "POST" && this->_hasContentLength && !this->_wantsClose
        && statusCode != 408 && this->_declaredContentLength > this->_bodyBytesConsumed)
    {
        this->_isDraining = true;
        this->_drainRemaining = this->_declaredContentLength - this->_bodyBytesConsumed;
    }
    // Same reasoning, chunked case: _chunkedBodyConsumed is only ever
    // true once the chunks parser has genuinely reached the real
    // terminator (see parseBodyWithChunked() above and
    // drainChunkedBody() below) - a false here means either the body
    // hasn't been read at all yet, or it errored out partway through,
    // either way there could be more of the client's declared body
    // still on the wire.
    else if (this->_method == "POST" && !this->_chunkedBodyConsumed && !this->_wantsClose
        && statusCode != 408)
    {
        map<string, string>::iterator it = _headers.find("transfer-encoding");
        if (it != _headers.end() && it->second == "chunked")
        {
            this->_chunks.discardFromNowOn();
            this->_isChunkedDraining = true;
            // !_isReadingBody means this error fired before
            // parseBodyWithChunked() ever ran for this request (e.g.
            // a header-validation error, or the Content-Length +
            // Transfer-Encoding conflict below) - _chunks is still
            // untouched, and _requestBuffer holds body bytes from
            // this same read that nothing has looked at yet. Walk
            // them right now: otherwise this only advances on a new
            // socket read, which may never come if the client
            // considers its request already fully sent (the common
            // case for a small request that fits in one packet) -
            // the connection would then just sit open, undrained,
            // until the idle-timeout scan eventually force-closes it.
            // (If _isReadingBody is already true, _chunks.parse() was
            // just called on this same _requestBuffer by
            // parseBodyWithChunked() itself - re-feeding it here
            // would double-process already-consumed bytes.)
            if (!this->_isReadingBody && !this->_requestBuffer.empty())
            {
                try
                {
                    this->_chunks.parse(this->_requestBuffer, (int)this->_requestBuffer.length());
                }
                catch (int drainStatus)
                {
                    if (drainStatus == 201)
                        this->_chunkedBodyConsumed = true;
                    this->_isChunkedDraining = false;
                }
            }
        }
    }
    spdlog::debug("{} {} -> {} ({})", _method, _tmpRequestTarget, statusCode, statusMessage);
    throw  statusCode;
}

bool Request::isDraining() const
{
    return this->_isDraining || this->_isChunkedDraining;
}

bool Request::isDrainTimedOut() const
{
    return this->_drainTimedOut;
}

bool Request::hasKnownBodyLength() const
{
    if (this->_hasContentLength)
        return true;
    map<string, string>::const_iterator it = _headers.find("transfer-encoding");
    return it != _headers.end() && it->second == "chunked";
}

// Only meaningful once any chunked draining this connection needed
// has already finished - isDraining() is false by then, which is the
// only point either caller (Webserver::start()'s reuse decision, and
// its periodic drain-timeout-abort scan) actually consults this.
bool Request::chunkedBodyFailedToDrain() const
{
    map<string, string>::const_iterator it = _headers.find("transfer-encoding");
    bool wasChunked = it != _headers.end() && it->second == "chunked";
    return wasChunked && !this->_chunkedBodyConsumed;
}

void Request::abortDraining()
{
    this->_isDraining = false;
    this->_isChunkedDraining = false;
    this->_drainTimedOut = true;
}

void Request::setTimeout()
{
    this->_statusCode = 408;
    this->_isRequestFinished = true;
    this->isErrorCode = true;
    if (this->_outfileIsCreated)
    {
        remove(this->_filePath.c_str());
        this->_outfile.close();
    }
    spdlog::debug("{} {} -> 408 (Request Timeout)", _method, _tmpRequestTarget);
}

bool Request::getWantsClose() const
{
    return this->_wantsClose;
}

Server *Request::getServer() const
{
    return this->_server;
}

bool Request::getIsRequestFinished() const
{
    return this->_isRequestFinished;
}

string Request::getMethod() const
{
    return this->_method;
}

string Request::getRequestTarget() const
{
    return this->_requestTarget;
}

string Request::getHttpVersion() const
{
    return this->_httpVersion;
}
int Request::getStatusCode() const
{
    return this->_statusCode;
}
map<string, string> Request::getHeaders() const
{
    return this->_headers;
}

vector<string> Request::split(string str, string delimiter)
{
    vector<string> tokens;
    size_t pos = str.find(delimiter);
    while (pos != string::npos)
    {
        tokens.push_back(str.substr(0, pos));
        str.erase(0, pos + delimiter.length());
        pos = str.find(delimiter);
    }
    tokens.push_back(str);
    return tokens;
}

string Request::toLowerCase(const string &str)
{
    string lowerCaseStr = str;
    transform(lowerCaseStr.begin(), lowerCaseStr.end(), lowerCaseStr.begin(),
              [](unsigned char c) { return tolower(c); });
    return lowerCaseStr;
}

void Request::trim(string& str)
{
    size_t start = str.find_first_not_of(' ');
    size_t end = str.find_last_not_of(' ');
    str = (start == string::npos) ? "" : str.substr(start, end - start + 1);
}