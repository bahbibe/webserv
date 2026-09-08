#include "../../inc/Response.hpp"

Response::Response():_flag(false),_isfinished(false),_defaultError(false),_isErrorCode(false),_cgiAutoIndex(false),_isHead(false),_keepAlive(false),_deleteDone(false) ,_fdSocket(0), _statusCode(0), _bytesSent(0), _headerOffset(0), _bodyOffset(0), _pendingBodyLen(0), env(NULL), pid(0), _isCGI(false)
{
    saveStatus();
}

double Response::fileSize(string path)
{
    struct stat metadata;
    stat(path.c_str(), &metadata);
    return metadata.st_size;
}

void Response::CGI(Request &req)
{
    if (this->_isCGI == false)
    {
        this->_isCGI = true;
        this->start = time(NULL);
        this->_randPath = "/tmp/" + toSting(rand());
        fillEnv(req);
        cout.flush();
        this->pid = fork();
        if (this->pid == 0)
        {
            const char *argv[] = {this->_cgiPath.c_str(), this->_absPath.c_str() ,NULL};
            freopen(this->_randPath.c_str(), "w", stdout);
            if (this->_method == "GET")
                close(0);
            else
                freopen(req.directives.cgiFileName.c_str(), "r", stdin);
            size_t slash = this->_absPath.rfind('/');
            if (slash != string::npos)
                chdir(this->_absPath.substr(0, slash).c_str());
            execve(argv[0], (char* const*)argv, this->env);
            cerr << "execve: " << strerror(errno) << "\n";
            exit(127);
        }
    }
    int status = 0;
    pid_t wPid = waitpid(pid, &status, WNOHANG);
    double elapsed = CLOCKWORK(this->start);
    if (wPid == -1  || wPid > 0 || elapsed > CGI_TIMEOUT)
    {
        char buffer[1024] = {0};
        unsigned long pos;
        string str;
        stringstream ss;

        this->_cgiAutoIndex = false;
        this->_path = this->_randPath;
        this->file.open(this->_path.c_str(), ios::in | ios::binary);
        this->_flag = true;
        this->_isCGI = true;
        if (status != 0 || elapsed > CGI_TIMEOUT)
        {
            this->_isErrorCode = true;
            if (status != 0)
                this->_statusCode = 500;
            else
            {
                kill(this->pid, SIGKILL);
                waitpid(pid, 0, 0);
                this->_statusCode = 504;
            }
            file.close();
            if (this->_isCGI == true)
            {
                freeEnv(this->env);
                this->env = NULL;
                remove(this->_path.c_str());
                remove(req.directives.cgiFileName.c_str());
            }
            checkErrors(req);
        }
        else
        {
            if (file.is_open())
            {
                file.read(buffer, 1023);
                ss << buffer;
                str = ss.str();
                if ((pos = str.find("\r\n\r\n")) != string::npos)
                {
                    int s = (str.length() - (pos + 4)) * -1;
                    this->_cgiHeader = str.substr(0, pos + 2);
                    this->file.clear();
                    this->file.seekg(s , ios::cur);
                }
                else
                {
                    file.clear();
                    file.seekg(0 ,ios::beg);
                }
            }
            this->_statusCode = 200;
            this->_method = "GET";
            SendHeader();
            if (headerSent())
                GET(req);
        }
    }
}

void Response::GET(Request &request)
{
    signal(SIGPIPE, SIG_IGN);
    if (this->_isHead)
    {
        if (file.is_open())
            file.close();
        this->_flag = false;
        this->_isfinished = true;
        if (this->_isCGI == true)
        {
            freeEnv(this->env);
            this->env = NULL;
            remove(this->_path.c_str());
            remove(request.directives.cgiFileName.c_str());
        }
        this->_isCGI = false;
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
            if (this->_isCGI == true)
            {
                freeEnv(this->env);
                this->env = NULL;
                remove(this->_path.c_str());
                remove(request.directives.cgiFileName.c_str());
            }
            this->_isCGI = false;
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
            if (this->_isCGI == true)
            {
                freeEnv(this->env);
                this->env = NULL;
                remove(this->_path.c_str());
                remove(request.directives.cgiFileName.c_str());
            }
            this->_isCGI = false;
        }
    }
}

void Response::DELETE(string path)
{
    if (is_adir(path))
    {
        DIR *dir = opendir(path.c_str());
        if (dir)
        {
            struct dirent *dp;
            while ((dp = readdir(dir)) != NULL)
            {
                if (strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0)
                {
                    string np = path + "/" + string(dp->d_name);
                    if (dp->d_type == DT_DIR)
                        DELETE(np);
                    else
                    {

                        if (access(np.c_str(), W_OK) != -1)
                            remove(np.c_str());
                        else
                        {
                            this->_isErrorCode = true;
                            this->_statusCode = 403;
                        }
                    }
                }

            }
            closedir(dir);
            remove(path.c_str());
        }
    }
    else if (access(path.c_str(), F_OK) != -1)
    {
        ifstream file;
        file.open(path.c_str(), ios::binary);
        if (file.is_open())
            remove(path.c_str());
        else
        {
            this->_isErrorCode = true;
            this->_statusCode = 403;
        }
        
    }
    else
    {
        this->_isErrorCode = true;
        this->_statusCode = 404;
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
void Response::sendResponse(Request &request, int fdSocket)
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
            GET(request);
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
            CGI(request);
        }
        else
        {
            this->_isErrorCode = true;
            this->_statusCode = 404;
            checkErrors(request);
            if (!this->_defaultError && headerSent())
                GET(request);
        }
    }
    else if ((this->_method == "GET" || this->_method == "HEAD") && !this->_isErrorCode)
    {
        if (!this->_flag)
            checks(request);
        if (this->_cgiAutoIndex)
            CGI(request);
        else if (!this->_defaultError && headerSent())
            GET(request);
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
            CGI(request);
        else if (!this->_defaultError && headerSent())
            GET(request);
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
            GET(request);
    }
}

string Response::resolveCgiPath(Request &request) const
{
    string ext = (this->_path.rfind(".php") != string::npos) ? "php" : "py";
    map<string, string>::const_iterator it = request.directives.cgiPaths.find(ext);
    if (it != request.directives.cgiPaths.end())
        return it->second;
    return ext == "php" ? "/usr/bin/php-cgi" : "/usr/bin/python3";
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

void Response::checkAutoInedx(Request &request)
{
        for (size_t i = 0; i < request.directives.indexs.size(); i++)
        {
            string index = this->_path + request.directives.indexs[i];
            this->file.open(index.c_str(), ios::in | ios::binary);
            if (file.is_open())
            {
                this->_path = index;
                if (request.directives.isCgiAllowed && (this->_path.rfind(".php") != string::npos || this->_path.rfind(".py") != string::npos))
                {
                    this->_absPath = this->_path;
                    file.close();
                    this->_cgiPath = resolveCgiPath(request);
                   this->_cgiAutoIndex = true;
                }
                else
                {
                    this->_statusCode = 200;
                    this->_flag = true;
                    findeContentType(request);
                    SendHeader();
                }
                return;
            }
        }
    if (request.directives.autoindex)
        tree_dir();
    else
    {
        request.isErrorCode = 1;
        this->_statusCode = 403;
        checkErrors(request);
    }
}

void Response::tree_dir()
{
    DIR *dir = opendir(this->_path.c_str());
    if (dir)
    {
        this->_contentType = "text/html";
        SendHeader();
        if (this->_body.empty())
        {
            struct dirent *dp;
            string name;
            string body = "<html><head></head><body><ul>";
            while ((dp = readdir(dir)))
            {
                name = dp->d_name;
                body += "<li><a href='"+ name  +"'>"  + name +"</a></li>";
            }
            stringstream ss;
            ss << hex << body.length();
            this->_body = ss.str() + "\r\n";
            this->_body += body + "\r\n";
            this->_body += "0\r\n\r\n";
            this->_bodyOffset = 0;
            this->_pendingBodyLen = body.length();
        }
        closedir(dir);
        if (this->_isHead)
        {
            this->_defaultError = true;
            this->_isfinished = true;
            this->_flag = false;
        }
        else if (flushBody())
        {
            this->_bytesSent += this->_pendingBodyLen;
            this->_defaultError = true;
            this->_isfinished = true;
            this->_flag = false;
        }
    }
}

string Response::getErrorPage(Request &request, int statusCode)
{
    map<string, string>::iterator it;
    it = request.directives.errorPages.find(toSting(statusCode));
    if (it != request.directives.errorPages.end())
        return it->second;
    return "default";
}

void Response::checkErrors(Request &request)
{
    this->_path = getErrorPage(request, this->_statusCode);
    if (!this->_flag)
        file.open(this->_path.c_str(), ios::in | ios::binary);
    if (!file.is_open() || this->_path == "default")
    {
        this->_contentType = "text/html";
        SendHeader();
        if (this->_body.empty())
        {
            string error;
            stringstream ss;
            map<int, string>::iterator it;
            it = this->status.find(this->_statusCode);
            error = templateError(it != this->status.end() ? it->second : "500 Internal Server Error");
            ss << hex << error.length();
            this->_body = ss.str() + "\r\n";
            this->_body += error + "\r\n";
            this->_body += "0\r\n\r\n";
            this->_bodyOffset = 0;
            this->_pendingBodyLen = error.length();
        }
        if (this->_isHead)
        {
            this->_defaultError = true;
            this->_isfinished = true;
        }
        else if (flushBody())
        {
            this->_bytesSent += this->_pendingBodyLen;
            this->_defaultError = true;
            this->_isfinished = true;
        }
    }
    else if (file.good() && !this->_flag)
    {
        findeContentType(request);
        SendHeader();
        this->_flag = true;
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
    ssize_t n = write(this->_fdSocket, this->_header.c_str() + this->_headerOffset,
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
    ssize_t n = write(this->_fdSocket, this->_body.c_str() + this->_bodyOffset,
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

string Response::templateError(string errorType)
{
    string errorBody;

    errorBody = "<html><head><title>"+ errorType +"</title></head>";
    errorBody += "<body><center><h1>"+ errorType + "</h1></center><hr><center>M0BLACK</center></body>";
    return errorBody;
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

int Response::fillEnv(Request &req)
{
    vector<string> vars;
    string requestUri = req.directives.requestTarget;
    if (!req.directives.queryString.empty())
        requestUri += "?" + req.directives.queryString;

    vars.push_back("REQUEST_METHOD=" + this->_method);
    vars.push_back("QUERY_STRING=" + req.directives.queryString);
    vars.push_back("REDIRECT_STATUS=200");
    vars.push_back("PATH_INFO=");
    vars.push_back("SCRIPT_FILENAME=" + this->_absPath);
    vars.push_back("SCRIPT_NAME=" + req.directives.requestTarget);
    vars.push_back("REQUEST_URI=" + requestUri);
    vars.push_back("GATEWAY_INTERFACE=CGI/1.1");
    vars.push_back("SERVER_PROTOCOL=" + req.getHttpVersion());
    vars.push_back("SERVER_SOFTWARE=webserv/1.0");
    vars.push_back("REMOTE_ADDR=" + req._clientIp);
    vars.push_back("CONTENT_TYPE=" + req.directives.contentType);
    if (req.getServer())
    {
        vars.push_back("SERVER_NAME=" + req.getServer()->getHost());
        vars.push_back("SERVER_PORT=" + req.getServer()->getPort());
    }
    if (this->_method == "GET" || this->_method == "HEAD")
        vars.push_back("CONTENT_LENGTH=0");
    else
    {
        double size = fileSize(req.directives.cgiFileName);
        vars.push_back("CONTENT_LENGTH=" + toSting(size));
    }
    map<string, string> headers = req.getHeaders();
    for (map<string, string>::iterator it = headers.begin(); it != headers.end(); ++it)
    {
        string name = it->first;
        if (name == "content-type" || name == "content-length")
            continue;
        string envName = "HTTP_";
        for (size_t i = 0; i < name.length(); i++)
            envName += (name[i] == '-') ? '_' : (char)toupper((unsigned char)name[i]);
        vars.push_back(envName + "=" + it->second);
    }

    this->env = new char *[vars.size() + 1];
    for (size_t i = 0; i < vars.size(); i++)
        env[i] = dupCStr(vars[i].c_str());
    env[vars.size()] = NULL;
    return 1;
}

char *Response::dupCStr(const char *s) const
{
    size_t len = strlen(s);
    char *copy = new char[len + 1];
    memcpy(copy, s, len + 1);
    return copy;
}

void Response::freeEnv(char **env)
{
    for (int i = 0; env[i]; i++)
        delete[] env[i];
    delete[] env;
}

char **Response::dupEnv(char * const *env) const
{
    if (!env)
        return NULL;
    int n = 0;
    while (env[n])
        n++;
    char **copy = new char *[n + 1];
    for (int i = 0; i < n; i++)
        copy[i] = dupCStr(env[i]);
    copy[n] = NULL;
    return copy;
}

Response::Response(const Response &other) : env(NULL)
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
        if (this->env)
            freeEnv(this->env);
        this->env = dupEnv(other.env);
        this->_cgiAutoIndex = other._cgiAutoIndex;
        this->start = other.start;
        this->_randPath = other._randPath;
        this->_deleteDone = other._deleteDone;
        this->_headerOffset = other._headerOffset;
        this->_bodyOffset = other._bodyOffset;
        this->_pendingBodyLen = other._pendingBodyLen;
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

Response::~Response()
{
    if (this->env)
        freeEnv(this->env);
}