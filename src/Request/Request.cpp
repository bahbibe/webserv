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
        this->_location = other._location;
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

        this->_headersBuffer = other._headersBuffer;
        this->_rest = other._rest;

        this->_boundaries = other._boundaries;
        this->_chunks = other._chunks;

        this->bufferSize = other.bufferSize;

        this->_isCgi = other._isCgi;
        this->servers = other.servers;
    }
    return *this;
}

Request::Request() : _socketFd(0), _lineCount(0), _statusCode(200), _isRequestFinished(false),
    _isFoundCRLF(false),  _outfileIsCreated(false), _bodyLength(0),
    _isReadingBody(false), _contentLength(0), _isBodyBoundary(false), _isCgi(false), isErrorCode(false) , _ready(false)
{
    this->_readBytes = 0;
    this->_location = NULL;
    memset(_buffer, 0, BUFFER_SIZE);
    this->bufferSize = BUFFER_SIZE;
    this->_start = 0;
}

Request::Request(Server* server, int socketFd, vector<Server> servers) : _socketFd(socketFd), _lineCount(0), _statusCode(200), _isRequestFinished(false),
    _isFoundCRLF(false),  _outfileIsCreated(false), _bodyLength(0),
    _isReadingBody(false), _contentLength(0), _isBodyBoundary(false), _isCgi(false), isErrorCode(false), _ready(false)
{
    this->servers = servers;
    this->_server = server;
    this->_readBytes = 0;
    this->_location = NULL;
    memset(_buffer, 0, BUFFER_SIZE);
    this->bufferSize = BUFFER_SIZE;
    this->_start = 0;
}

void Request::readRequest()
{
    try {
        _start = clock();
        _requestBuffer.clear();
        _readBytes = read(_socketFd, _buffer, bufferSize);
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
    size_t pos = _headersBuffer.find("\r\n\r\n");
    if (pos == string::npos)
        return;
    _ready = true;
    _headersBuffer = _headersBuffer.substr(0, pos);
    _readBytes -= _headersBuffer.length() + 4;
    _requestBuffer.erase(0, _headersBuffer.length() + 4);
    parseRequestLine();
    parseHeaders();
    _headersBuffer.clear();
    parseBody();
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
    if (this->_method != "GET" && this->_method != "POST" && this->_method != "DELETE")
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
    map<string, Location *> locations = this->_server->getLocations();
    map<string, Location *>::iterator itb = locations.begin();
    map<string, Location *>::iterator ite = locations.end();
    while (locations.size() > 0 && ite-- != itb)
    {
        if (!this->_requestTarget.compare(0, ite->first.length(), ite->first))
        {
            this->_requestTarget.erase(0, ite->first.length());
            return ite->second;
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
    map<string, Location *> locations = this->_server->getLocations();
    Location defaultLocation;
    _location = this->findLocation();
    // if (_location == NULL && locations.size() > 0)
    //     setStatusCode(404, "Not Found");
    // else 
    if (_location == NULL)
    {
        defaultLocation.setMethods("GET");
        defaultLocation.setRoot(_server->getRoot());
        _location = &defaultLocation;
    }
    vector<string> locationMethods = _location->getMethods();
    if (locationMethods.size() > 0)
    {
        vector<string>::iterator itb = locationMethods.begin();
        vector<string>::iterator ite = locationMethods.end();
        if (find(itb, ite, _method) == ite)
            setStatusCode(405, "Method Not Allowed");
    }
    directives.isUploadAllowed = _location->getUpload();
    directives.uploadPath = _location->getUploadPath();
    directives.isCgiAllowed = _location->getCgi();
    directives.cgiUploadPath = _location->getCgiUploadPath();
    directives.returnRedirect = _location->getReturn();
    directives.autoindex = _location->getAutoindex();
    directives.serverRoot = _location->getRoot();
    directives.indexs = _location->getIndexs();
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
    if (realpath(directives.requestedFile.c_str(), realPath) != NULL)
    {
        string realPathStr = realPath;
        if (realPathStr.find("WWW") == string::npos)
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
        setStatusCode(statusCode, "Chunks Status Code");
    }
}

void Request::setStatusCode(int statusCode, string statusMessage)
{
    // this->printRequest();
    this->_statusCode = statusCode;
    this->_isRequestFinished = true;
    stringstream ss;
    ss << statusCode;
    if (statusCode >= 400)
        this->isErrorCode = true;
    this->_statusMessage = statusCode >= 400 ? RED + statusMessage + ": " + ss.str() + RESET : GREEN + statusMessage + ": " + ss.str() + RESET;
    // cout << GREEN << _tmpRequestTarget << " " << _method  << " " << _statusMessage << RESET << endl;
    throw  statusCode;
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
    this->_statusMessage = RED "Request Timeout: 408" RESET;
    // cout << GREEN << _tmpRequestTarget << " " << _method  << " " << _statusMessage << RESET << endl;
}

void Request::printRequest()
{
    cout << GREEN "=====================Request=================" RESET << endl;
    cout << "Method: " << _method << endl;
    cout << "Request Target: " << _requestTarget << endl;
    cout << "HTTP Version: " << _httpVersion << endl;
    cout << "Headers size: " << _headers.size() << endl;
    cout << "Boundary: " << _boundary << endl;
    cout << "isCgi: " << _isCgi << endl;
    cout << "Headers: " << endl;
    map<string, string>::iterator it = _headers.begin();
    for (; it != _headers.end(); it++)
        cout << it->first << ": " << it->second << endl;
    cout << BLUE "=====================Directives=================" RESET << endl;
    cout << "Requested File Path: " << directives.requestedFile << endl;
    cout << "Root: " << directives.serverRoot << endl;
    cout << "Client Max Body Size: " << directives.clientMaxBodySize << endl;
    cout << "Autoindex: " << directives.autoindex << endl;
    cout << "Is Upload Allowed: " << directives.isUploadAllowed << endl;
    cout << "Upload Path: " << directives.uploadPath << endl;
    cout << "Is Cgi Allowed: " << directives.isCgiAllowed << endl;
    cout << "Cgi upload Path: " << directives.cgiUploadPath << endl;
    cout << "Return Redirect: " << directives.returnRedirect << endl;
    cout << "requestTarget: " << directives.requestTarget << endl;
    cout << "queryString: " << directives.queryString << endl;
    cout << "httpCookie: " << directives.httpCookie << endl;
    cout << "httpAccept: " << directives.httpAccept << endl;
    cout << "CgiFileName: " << directives.cgiFileName << endl;
    cout << "Content Type: " << directives.contentType << endl;
    cout << "Boundary: " << directives.boundary << endl;
    cout << "Content Length: " << directives.contentLength << endl;
    cout << "Is CGI: " << directives.isCGI << endl;
    cout << BLUE "=====================Directives=================" RESET << endl;
    cout << GREEN "=====================Request=================" RESET << endl;
}

bool Request::getIsRequestFinished() const
{
    return this->_isRequestFinished;
}

string Request::getStatusMessage() const
{
    return this->_statusMessage;
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

Location *Request::getLocation() const
{
    return this->_location;
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
    string lowerCaseStr = "";
    for (size_t i = 0; i < str.length(); i++)
        lowerCaseStr += tolower(str[i]);
    return lowerCaseStr;
}

void Request::trim(string& str)
{
    while (str.length() > 0 && str[0] == ' ')
        str.erase(0, 1);
    while (str.length() > 0 && str[str.length() - 1] == ' ')
        str.erase(str.length() - 1, 1);
}