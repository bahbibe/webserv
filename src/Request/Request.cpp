#include "../../inc/Request.hpp"

Request::~Request()
{
    if (this->_outfileIsCreated)
    {
        this->_outfile->close();
        delete this->_outfile;
    }
}
Request::Request(const Request &other)
{
    *this = other;
}


Request &Request::operator=(const Request &other)
{
    if (this != &other)
    {
        this->_method = other._method;
        this->_requestTarget = other._requestTarget;
        this->_httpVersion = other._httpVersion;
        this->_headers = other._headers;
        this->_fileFullPath = other._fileFullPath;
        this->_filePath = other._filePath;
        this->_socketFd = other._socketFd;
        this->_server = other._server;
        this->_location = other._location;
        this->_lineCount = other._lineCount;
        this->_statusCode = other._statusCode;
        this->_isRequestFinished = other._isRequestFinished;
        this->_isFoundCRLF = other._isFoundCRLF;
        this->_outfile = other._outfile;
        this->_outfileIsCreated = other._outfileIsCreated;
        this->_bodyLength = other._bodyLength;
        this->_isReadingBody = other._isReadingBody;
        this->_contentLength = other._contentLength;
        this->isErrorCode = other.isErrorCode;
    }
    return *this;
}
Request::Request(Server* server, int socketFd) : _socketFd(socketFd) , _lineCount(0), _statusCode(200), _isRequestFinished(false),
    _isFoundCRLF(false), _outfile(NULL), _outfileIsCreated(false), _bodyLength(0),
    _isReadingBody(false), _contentLength(0), isErrorCode(false)
{
    this->_server = server;
    this->_location = NULL;
    memset(_buffer, 0, BUFFER_SIZE);
}

void Request::readRequest()
{
    try {
        int readBytes = read(_socketFd, _buffer, BUFFER_SIZE);
        if (readBytes == -1)
            throw Server::ServerException(ERR "Failed to read from socket");
        _buffer[readBytes] = '\0';
        this->parseRequest(_buffer);
        if (!this->_isRequestFinished)
            return;
    } catch (int statusCode)
    {
        std::cerr << this->getStatusMessage() << '\n';
        return;
    }
}

void Request::setContentLength(string contentLength)
{
    for (size_t i = 0; i < contentLength.length(); i++)
        if (!isdigit(contentLength[i]))
            setStatusCode(400, "Invalid Content-Length");
    this->_contentLength = atoi(contentLength.c_str());
}

Location* Request::findLocation()
{
    map<string, Location *> locations = this->_server->getLocations();
    map<string, Location *>::iterator itb = locations.begin();
    map<string, Location *>::iterator ite = locations.end();
    while (ite-- != itb)
    {
        if (!this->_requestTarget.compare(0, ite->first.length(), ite->first))
        {
            this->_requestTarget.erase(0, ite->first.length());
            return ite->second;
        }
    }
    return NULL;
}

void Request::setServer()
{
    //! TODO: first find the server according to the host header
    this->_host = _server->getHost();
    this->_port = _server->getPort();
    this->_serverRoot = _server->getRoot();
    this->_clientMaxBodySize = _server->getClientMaxBodySize();
    this->_autoindex = _server->getAutoindex();
    this->_errorPages = _server->getErrorPages();
    this->_indexs = _server->getIndexs();
    this->_serverNames = _server->getServerNames();
    _location = this->findLocation();
    if (_location == NULL)
        setStatusCode(404, "Not Found");
    if (!_location->getReturn().empty())
        setStatusCode(301, "Moved Permanently");
    cout << GREEN "Location: " RESET << endl;
    _location->print();
    cout << GREEN "END location" RESET << endl;
    vector<string> locationMethods = _location->getMethods();
    if (locationMethods.size() > 0)
    {
        vector<string>::iterator itb = locationMethods.begin();
        vector<string>::iterator ite = locationMethods.end();
        if (find(itb, ite, _method) == ite)
            setStatusCode(405, "Method Not Allowed");
    }
    // TODO: override the server directives with the location directives
    this->_isUploadAllowed = _location->getUpload();
    this->_uploadPath = _location->getUploadPath();
    this->_isCgiAllowed = _location->getCgi();
    this->returnRedirect = _location->getReturn();
    if (_location->getIndexs().size() > 0)
        this->_indexs = _location->getIndexs();
    if (!_location->getRoot().empty())
        this->_serverRoot = _location->getRoot();
    this->_fileFullPath = this->_serverRoot + this->_requestTarget;
}

void Request::validateRequest()
{
    this->setServer();

    map<string, string>::iterator it = _headers.find("host");
    if (it == _headers.end() || it->second.length() == 0)
        setStatusCode(400, "No Host Header");
    if (_headers.find("transfer-encoding") != _headers.end() && _headers["transfer-encoding"] != "chunked")
        setStatusCode(501, "Unsupported Transfer-Encoding");
    if (_headers.find("content-length") != _headers.end() && _headers.find("transfer-encoding") != _headers.end())
        setStatusCode(400, "Both Content-Length and Transfer-Encoding are present");
    if (_method == "POST")
    {
        if (_headers.find("content-length") == _headers.end() && _headers.find("transfer-encoding") == _headers.end())
            setStatusCode(400, "Length Required");
        setContentLength(_headers["content-length"]);
        if (_headers.find("content-length") != _headers.end() && this->_contentLength > this->_server->getClientMaxBodySize())
            setStatusCode(413, "Request Entity Too Large");
    }
}

typedef pair<map<string, string>::iterator, bool> ret_type;

void Request::parseRequest(string buffer)
{
    if (this->_isReadingBody)
        return this->parseBody(buffer);
    while (buffer.length() > 0)
    {
        if (this->_lineCount == 0)
            this->parseRequestLine(buffer);
        else
        {
            size_t pos = buffer.find("\r\n");
            if (pos == string::npos)
                break;
            string header = buffer.substr(0, pos);
            buffer.erase(0, pos + 2);
            if (header.length() == 0)
                this->parseBody(buffer);
            if (header.length() != 0)
            {
                vector<string> headerTokens = this->split(header, ": ");
                string headerName = toLowerCase(headerTokens[0]);
                string headerValue = headerTokens.size() > 1 ? headerTokens[1] : "";
                trim(headerValue);
                ret_type ret = this->_headers.insert(pair<string, string>(headerName, headerValue));
                //! NOTE: random headers can be duplicated
                if (ret.second == false)
                    setStatusCode(400, "Duplicate Header");
            }
        }
    }
}

void Request::parseRequestLine(string& buffer)
{
    size_t pos = buffer.find("\r\n");
    if (pos == string::npos)
        this->setStatusCode(400, "Invalid Request Line");
    string requestLine = buffer.substr(0, pos);
    buffer.erase(0, pos + 2);
    vector<string> tokens = this->split(requestLine, " ");
    if (tokens.size() != 3)
        this->setStatusCode(400, "Invalid Request Line");
    this->_method = tokens[0];
    this->_requestTarget = tokens[1];
    this->_httpVersion = tokens[2];
    if (this->_method != "GET" && this->_method != "POST" && this->_method != "DELETE")
        setStatusCode(400, "Invalid Method");
    /*
        /// TODO: validate the request target
        /// TODO: parse the query string if exists && decode the uri
        /// - request target too long (2048)
    */
    if (this->_requestTarget.empty() || !Helpers::checkURICharSet(this->_requestTarget))
        setStatusCode(400, "Invalid Request Target");
    if (this->_requestTarget.length() > 1024)
        setStatusCode(414, "Request-URI Too Long");
    if (this->_httpVersion.length() != 8 || this->_httpVersion.substr(0, 5) != "HTTP/")
        setStatusCode(400, "Invalid HTTP Version");
    else if (this->_httpVersion.substr(5, 3) != "1.1")
        setStatusCode(505, "HTTP Version Not Supported");
    this->_lineCount++;
}

string Request::getMimeType(string contentType)
{
    // TODO: wait for the mime types
    if (contentType.find(";") != string::npos)
        contentType = contentType.substr(0, contentType.find(";"));
    map<string, vector<string> >::iterator it = this->_mimeTypes.find(contentType);
    if (it == this->_mimeTypes.end())
        return "txt";
    return it->second[0];
}

void Request::createOutfile()
{
    // TODO: upload the file in the upload folder
    // TODO: apply the extension to the file from the content type header
    string contentType = this->_headers["content-type"];
    string extension = this->getMimeType(contentType);
    this->_filePath = this->_uploadPath + "/file." + extension;
    this->_outfile = new fstream(this->_filePath.c_str(), ios::out);
    if (!this->_outfile->is_open())
        setStatusCode(500, "Failed to create file");
    this->_outfileIsCreated = true;
}

void Request::parseBody(string buffer)
{
    if (!this->_isReadingBody)
        this->validateRequest();
    this->_isReadingBody = true;
    //! NOTE: if the request has no body, the body length is 0 maybe should wait for the body 
    // if (buffer.length() == 0)
    //     setStatusCode(200, "OK");
    if (this->_method != "POST")
        setStatusCode(200, "OK");
    if (!this->_isUploadAllowed)
        setStatusCode(200, "OK But upload is not allowed so work of the cgi");
    if (!this->_outfileIsCreated)
        this->createOutfile();
    if (_headers.find("transfer-encoding") != _headers.end() && _headers["transfer-encoding"] == "chunked")
        parseBodyWithChunked(buffer);
    else if (_headers.find("content-length") != _headers.end())
        parseBodyWithContentLength(buffer);
}

void Request::parseBodyWithContentLength(string buffer)
{
    if (_contentLength == 0)
        setStatusCode(200, "OK");
    if (buffer.length() > _contentLength)
        buffer.erase(_contentLength, buffer.length() - _contentLength);
    if (buffer.length() < _contentLength)
    {
        this->_outfile->write(buffer.c_str(), buffer.length());
        this->_outfile->flush();
        _bodyLength += buffer.length();
        buffer.erase(0, buffer.length());
    }
    else
    {
        this->_outfile->write(buffer.c_str(), _contentLength);
        this->_outfile->flush();
        buffer.erase(0, _contentLength);
        _bodyLength += _contentLength;
    }
    // TODO: handle when the content length is bigger than the body length
    if (buffer.length() == 0 && _bodyLength == _contentLength)
        setStatusCode(201, "Created");
}

void Request::parseBodyWithChunked(string buffer)
{
    //* get size of chunk
    size_t pos = buffer.find("\r\n");
    if (pos == string::npos)
        return;
    string chunkSizeStr = buffer.substr(0, pos);
    buffer.erase(0, pos + 2);
    for (size_t i = 0; i < chunkSizeStr.length(); i++)
        if (!isxdigit(chunkSizeStr[i]))
            setStatusCode(400, "Invalid Chunk Size");
    size_t chunkSize = strtol(chunkSizeStr.c_str(), NULL, 16);
    if (chunkSize == 0)
        setStatusCode(201, "OK");
    //* get chunk
    if (buffer.length() < chunkSize)
    {
        this->_outfile->write(buffer.c_str(), buffer.length());
        this->_outfile->flush();
        buffer.erase(0, buffer.length());
    }
    else
    {
        this->_outfile->write(buffer.c_str(), chunkSize);
        this->_outfile->flush();
        buffer.erase(0, chunkSize);
    }
}

void Request::setStatusCode(int statusCode, string statusMessage)
{
    this->printRequest();
    this->_statusCode = statusCode;
    this->_isRequestFinished = true;
    stringstream ss;
    ss << statusCode;
    if (statusCode >= 400)
        this->isErrorCode = true;
    this->_statusMessage = statusCode >= 400 ? RED + statusMessage + ": " + ss.str() + RESET : GREEN + statusMessage + ": " + ss.str() + RESET;
    throw  statusCode;
}

void Request::printRequest()
{
    cout << GREEN "=====================Request=================" RESET << endl;
    cout << "Method: " << _method << endl;
    cout << "Request Target: " << _requestTarget << endl;
    cout << "HTTP Version: " << _httpVersion << endl;
    cout << "Headers size: " << _headers.size() << endl;
    cout << "Headers: " << endl;
    map<string, string>::iterator it = _headers.begin();
    for (; it != _headers.end(); it++)
        cout << it->first << ": " << it->second << endl;
    cout << "Requested File Path: " << _fileFullPath << endl;
    cout << "Root: " << _serverRoot << endl;
    cout << "Client Max Body Size: " << _clientMaxBodySize << endl;
    cout << "Autoindex: " << _autoindex << endl;
    cout << "Is Upload Allowed: " << _isUploadAllowed << endl;
    cout << "Upload Path: " << _uploadPath << endl;
    cout << "Is Cgi Allowed: " << _isCgiAllowed << endl;
    cout << "Return Redirect: " << returnRedirect << endl;
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

fstream *Request::getOutFile() const
{
    return this->_outfile;
}

string Request::getFileFullPath() const
{
    return this->_fileFullPath;
}

Location *Request::getLocation() const
{
    return this->_location;
}

bool Request::getAutoIndex() const
{
    return this->_autoindex;
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