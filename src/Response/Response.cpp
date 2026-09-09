#include "../../inc/Response.hpp"

Response::Response():_flag(false),_isfinished(false),_defaultError(false),_isErrorCode(false),_cgiAutoIndex(false),_isHead(false),_keepAlive(false),_deleteDone(false) ,_fdSocket(0), _statusCode(0), _bytesSent(0), _headerOffset(0), _bodyOffset(0), _pendingBodyLen(0), _cgiStdoutFd(-1), _cgiStdinFd(-1), _cgiHeaderParsed(false), _cgiReaped(false), _cgiTimedOut(false), _cgiDone(false), _cgiExitStatus(0), _cgiStdinChunkOffset(0), pid(0), _isCGI(false)
{
    saveStatus();
}

// Pure static-file streaming - CGI responses are relayed directly by
// CGI()/relayCgiOutput() via the pipe fds, never through here.
void Response::GET()
{
    signal(SIGPIPE, SIG_IGN);
    if (this->_isHead)
    {
        if (file.is_open())
            file.close();
        this->_flag = false;
        this->_isfinished = true;
        return;
    }
    if (this->_bodyOffset < this->_body.length())
    {
        if (!flushBody())
            return;
        this->_bytesSent += this->_pendingBodyLen;
        if (this->_pendingBodyLen == 0)
        {
            file.close();
            this->_flag = false;
            this->_isfinished = true;
        }
        return;
    }
    char _body1[BUFFERSIZE] = {0};
    file.read(_body1, 1023);
    if (file.gcount() > 0)
    {
        stringstream ss;
        ss << hex << file.gcount();
        this->_body = ss.str() + "\r\n";
        this->_body.append(_body1, file.gcount());
        this->_body.append("\r\n", 2);
        this->_bodyOffset = 0;
        this->_pendingBodyLen = (size_t)file.gcount();
        if (flushBody())
            this->_bytesSent += this->_pendingBodyLen;
    }
    else if (file.gcount() == 0)
    {
        this->_body = "0\r\n\r\n";
        this->_bodyOffset = 0;
        this->_pendingBodyLen = 0;
        if (flushBody())
        {
            file.close();
            this->_flag = false;
            this->_isfinished = true;
        }
    }
}

void Response::DELETE(string path)
{
    namespace fs = std::filesystem;
    error_code ec;
    if (!fs::exists(path, ec))
    {
        this->_isErrorCode = true;
        this->_statusCode = 404;
        return;
    }
    if (fs::is_directory(path, ec))
        fs::remove_all(path, ec);
    else
        fs::remove(path, ec);
    if (ec)
    {
        this->_isErrorCode = true;
        this->_statusCode = 403;
    }
}

void Response::initVars(Request &request, int fdSocket)
{
    if (!this->_flag)
    {
        this->_fdSocket = fdSocket;
        this->_path = request.directives.requestedFile;
        this->_statusCode = request.getStatusCode();
        this->_path = request.directives.requestedFile;
        this->_method = request.getMethod();
        this->_isHead = (this->_method == "HEAD");
        this->_keepAlive = (this->_method != "POST") && !request.getWantsClose() && (this->_statusCode != 408);
        this->_target = request.directives.requestTarget;
        this->_isErrorCode = request.isErrorCode;
        this->_absPath = request.directives.requestedFile;
        if (this->_target.empty())
            this->_target = "/";
    }
}
void Response::sendResponse(Request &request, int fdSocket, map<int, int> &cgiFdToClient)
{
    initVars(request, fdSocket);
    if (!this->_header.empty() && !headerSent())
    {
        if (!flushHeader())
            return;
    }
    if (this->_isErrorCode == true)
    {
        checkErrors(request);
        if (!this->_defaultError && headerSent())
            GET();
    }
    else if (this->_statusCode == 301 || (is_adir(this->_path) && this->_target[this->_target.length() - 1] != '/'))
    {
        if (this->_statusCode == 301)
            this->_path = request.directives.returnRedirect;
        else
        {
            this->_path = this->_target + "/";
            this->_statusCode = 301;
        }
        SendHeader();
        if (headerSent())
            this->_isfinished = true;
    }
    else if ((!this->_flag && request.directives.isCgiAllowed)
        && (this->_path.rfind(".php") != string::npos
        || this->_path.rfind(".py") != string::npos))
    {
        ifstream file;
        file.open(this->_path.c_str(), ios::binary);
        if (file.is_open())
        {
            file.close();
            this->_cgiPath = resolveCgiPath(request);
            CGI(request, cgiFdToClient);
        }
        else
        {
            this->_isErrorCode = true;
            this->_statusCode = 404;
            checkErrors(request);
            if (!this->_defaultError && headerSent())
                GET();
        }
    }
    else if ((this->_method == "GET" || this->_method == "HEAD") && !this->_isErrorCode)
    {
        if (!this->_flag)
            checks(request);
        if (this->_cgiAutoIndex)
            CGI(request, cgiFdToClient);
        else if (!this->_defaultError && headerSent())
            GET();
    }
    else if (this->_method == "POST" && !this->_isErrorCode)
    {
        if (!this->_flag)
        {
            if (is_adir(this->_path) && request.directives.isCGI == true)
                checkAutoInedx(request);
            else
                checkErrors(request);
        }
        if (this->_cgiAutoIndex)
            CGI(request, cgiFdToClient);
        else if (!this->_defaultError && headerSent())
            GET();
    }
    else if (this->_method == "DELETE" && !this->_isErrorCode)
    {
        if (!this->_deleteDone)
        {
            this->_statusCode = 204;
            DELETE(this->_path);
            this->_deleteDone = true;
        }
        checkErrors(request);
        if (!this->_defaultError && headerSent())
            GET();
    }
}

void Response::checks(Request &request)
{ 
    if (is_adir(this->_path) && !this->_flag)
        checkAutoInedx(request);
    else if (!this->_flag)
    {
        this->file.open(_path.c_str(), ios::in | ios::binary);
        if (!file.good())
        {
            this->_isErrorCode = 1;
            if (access(this->_path.c_str(), F_OK) != -1)
                this->_statusCode = 403;
            else
                this->_statusCode = 404;
            checkErrors(request);
        }
        else
        {
            findeContentType(request);
            SendHeader();
            this->_flag = true;
        }
    }
}

int Response::is_adir(string &path)
{
    struct stat metaData;
    return (stat(path.c_str(), &metaData) == 0 && (metaData.st_mode & S_IFDIR) ? 1 : 0);
}

void Response::saveStatus()
{
    this->status[200] = "200 OK";
    this->status[201] = "201 Created";
    this->status[204] = "204 No Content";
    this->status[301] = "301 Moved Permanently";
    this->status[400] = "400 Bad Request";
    this->status[403] = "403 Forbidden";
    this->status[404] = "404 Not Found";
    this->status[405] = "405 Method Not Allowed";
    this->status[408] = "408 Request Timeout";
    this->status[409] = "409 Conflict";
    this->status[411] = "411 Length Required";
    this->status[413] = "413 Content Too Large";
    this->status[414] = "414 URI Too Long";
    this->status[500] = "500 Internal Server Error";
    this->status[501] = "501 Not Implemented";
    this->status[504] = "504 Gateway Timeout";
    this->status[505] = "505 HTTP Version Not Supported";
}

void Response::SendHeader()
{
    if (this->_header.empty())
    {
        map<int,string>::iterator it;
        it = this->status.find(this->_statusCode);
        this->_header = "HTTP/1.1 " + (it != this->status.end() ? it->second : "500 Internal Server Error") +"\r\n";
        time_t now = time(NULL);
        char dateBuf[32];
        strftime(dateBuf, sizeof(dateBuf), "%a, %d %b %Y %H:%M:%S GMT", gmtime(&now));
        this->_header += "Date: " + string(dateBuf) + "\r\n";
        if(_statusCode == 301)
        {
            this->_header += "Location: " + this->_path +"\r\n";
            this->_header += this->_keepAlive ? "connection: keep-alive\r\n\r\n" : "connection: close\r\n\r\n";
        }
        else
        {
            if (this->_contentType.empty())
                this->_header += "Content-Type: text/html\r\n";
            else
                this->_header += "Content-Type: " + this->_contentType + "\r\n";
            this->_header += "Transfer-Encoding: chunked\r\n";
            if (this->_isCGI == true)
                this->_header += this->_cgiHeader;
            this->_header += this->_keepAlive ? "connection: keep-alive\r\n\r\n" : "connection: close\r\n\r\n";
        }
        this->_headerOffset = 0;
    }
    flushHeader();
}

bool Response::flushHeader()
{
    if (this->_headerOffset >= this->_header.length())
        return true;
    ssize_t n = tlsAwareWrite(this->_fdSocket, this->_header.c_str() + this->_headerOffset,
                       this->_header.length() - this->_headerOffset);
    if (n <= 0)
        return false;
    this->_headerOffset += (size_t)n;
    return this->_headerOffset >= this->_header.length();
}

bool Response::flushBody()
{
    if (this->_bodyOffset >= this->_body.length())
        return true;
    ssize_t n = tlsAwareWrite(this->_fdSocket, this->_body.c_str() + this->_bodyOffset,
                       this->_body.length() - this->_bodyOffset);
    if (n <= 0)
        return false;
    this->_bodyOffset += (size_t)n;
    return this->_bodyOffset >= this->_body.length();
}

bool Response::headerSent() const
{
    return this->_headerOffset >= this->_header.length();
}

void Response::findeContentType(Request &req)
{
    int idex= this->_path.rfind(".");
    string extention = this->_path.substr(idex + 1);
    map<string, string>::iterator it = req.directives.types.find(extention);
    if (it != req.directives.types.end())
        this->_contentType = it->second;
}

string Response::toSting(long long mun)
{
    stringstream ss;
    ss << mun;
    return ss.str();
}

Response::Response(const Response &other)
{
    *this = other;
}

Response &Response::operator=(const Response &other)
{
    if (this != &other)
    {
        this->_flag = other._flag;
        this->_isfinished = other._isfinished;
        this->_defaultError = other._defaultError;
        this->_isErrorCode = other._isErrorCode;
        this->_isHead = other._isHead;
        this->_keepAlive = other._keepAlive;
        this->_fdSocket = other._fdSocket;
        this->_statusCode = other._statusCode;
        this->_bytesSent = other._bytesSent;
        this->_method = other._method;
        this->_path = other._path;
        this->_contentType = other._contentType;
        this->_header = other._header;
        this->_target = other._target;
        this->_body = other._body;
        this->mime = other.mime;
        this->status = other.status;
        this->_isCGI = other._isCGI;
        this->_absPath = other._absPath;
        this->_cgiPath = other._cgiPath;
        this->_cgiHeader = other._cgiHeader;
        this->pid = other.pid;
        this->_cgiEnv = other._cgiEnv;
        this->_cgiAutoIndex = other._cgiAutoIndex;
        this->start = other.start;
        this->_deleteDone = other._deleteDone;
        this->_headerOffset = other._headerOffset;
        this->_bodyOffset = other._bodyOffset;
        this->_pendingBodyLen = other._pendingBodyLen;
        this->_cgiStdoutFd = other._cgiStdoutFd;
        this->_cgiStdinFd = other._cgiStdinFd;
        this->_cgiHeaderParsed = other._cgiHeaderParsed;
        this->_cgiReaped = other._cgiReaped;
        this->_cgiTimedOut = other._cgiTimedOut;
        this->_cgiDone = other._cgiDone;
        this->_cgiExitStatus = other._cgiExitStatus;
        this->_cgiOutBuf = other._cgiOutBuf;
        this->_cgiStdinChunk = other._cgiStdinChunk;
        this->_cgiStdinChunkOffset = other._cgiStdinChunkOffset;
    }
    return *this;
}

bool Response::getIsFinished() const
{
    return this->_isfinished;
}

bool Response::getKeepAlive() const
{
    return this->_keepAlive;
}

size_t Response::getBytesSent() const
{
    return this->_bytesSent;
}

int Response::getStatusCode() const
{
    return this->_statusCode;
}

