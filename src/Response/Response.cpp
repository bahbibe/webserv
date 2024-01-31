#include "../../inc/Response.hpp"

Response::Response():_flag(false),_isfinished(false),_defaultError(false),_isErrorCode(false),_isCGI(false),_cgiAutoIndex(false) ,_fdSocket(0), _statusCode(0)
{
    saveStatus();
}

// Response::Response(Request request, int fdSocket)
// {
//     this->_flag = false;
//     this->_isfinished = false;
//     this->_isErrorCode = false;
//     this->_defaultError = false;
//     this->_fdSocket = fdSocket;
//     this->_path = request.getRequestTarget();
//     this->_statusCode = request.getStatusCode();
//     this->_path = request.directives.requestedFile;
//     this->_method = request.getMethod();
//     this->_target = request.directives.requestTarget;
//     this->_isErrorCode = request.isErrorCode;
//     saveStatus();
//     cout << "***Target: " << this->_target << endl;
//     cout << "***Method: " << this->_method << endl;
//     cout << "***Is Error: " << this->_isErrorCode  << endl;
//     cout << "***Path: " << this->_path << endl;
// }

double Response::fileSize(string path)
{
    struct stat metadata;
    stat(path.c_str(), &metadata);
    return metadata.st_size;
}

void Response::CGI(Request &req)
{
    int status ;
    cout << RED"=========================" << this->_isCGI <<"=============="<< RESET <<  endl;
    if (this->_isCGI == false)
    {
        cout << RED "****************************CGI executing**************8888888" << RESET << endl;
        this->_isCGI = true;
        this->start = clock();
        srand(time(NULL));
        this->_randPath = "CGI/" + toSting(rand());
        cout << "AbsPth: " << this->_absPath << endl;
        cout << "pathCGI: " << this->_path << endl;
        fillEnv(req);
        cout << this->_cgiPath << endl;
        pipe(fd);
        this->pid = fork();
        if (this->pid == 0)
        {
            const char *argv[] = {this->_cgiPath.c_str(), this->_absPath.c_str() ,NULL};
            close(fd[0]);
            close(fd[1]);
            freopen(this->_randPath.c_str(), "w", stdout);
            if (this->_method == "GET")
                close(0);
            else
                freopen(req.directives.cgiFileName.c_str(), "r", stdin);
            execve(argv[0], (char* const*)argv, this->env);
            perror("execve");
            exit(127);
        }
    }

    pid_t wPid = waitpid(pid, &status, WNOHANG);
    clock_t end = clock();
    cout << "Start: " << this->start << endl;
    cout << "End: " << end << endl;
    double time = (double)(end - this->start) / (double)CLOCKS_PER_SEC;
    cout << "Time: " << time << endl;
    cout << "WAITPID: " <<  wPid << endl;
    if (wPid == -1  || wPid > 0 || time > 5)
    {
        char buffer[1024];
        unsigned long pos;
        string str;
        stringstream ss;

        cout << "Status: " << status << endl;
        kill(pid, SIGTERM);
        this->_cgiAutoIndex = false;
        this->_path = this->_randPath;
        this->file.open(this->_path.c_str(), ios::in | ios::binary);
        this->_flag = true;
        this->_isCGI = true;
        if (status != 0 || time > 5)
        {
            this->_isErrorCode = true;
            if (status != 0)
                this->_statusCode = 500;
            else
                this->_statusCode = 504;
            file.close();
            checkErrors(req);
        }
        else if (file.is_open())
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
        if (!this->_defaultError)
        {
            this->_statusCode = 200;
            this->_method = "GET";
            SendHeader();
            GET(req);
        }
    }
}

void Response::GET(Request &request)
{
    (void)request;
    signal(SIGPIPE, SIG_IGN);
    char _body1[BUFFERSIZE] = {0};
    file.read(_body1, 1023);
    if (file.gcount() > 0)
    {
        cout << GREEN "====>start reading<====\n" RESET;
        stringstream ss;
        ss << hex << file.gcount();
        this->_body = ss.str() + "\r\n";
        this->_body.append(_body1, file.gcount());
        this->_body.append("\r\n", 2);
        int n = write(this->_fdSocket, this->_body.c_str(),  this->_body.length());
        perror("ERROR");
        if (n == -1)
            cout << "write failed...!!!!\n";
    }
    else if (file.gcount() == 0)
    {
        this->_body = "0\r\n\r\n";
        write(this->_fdSocket, this->_body.c_str(),   this->_body.length());
        file.close();
        this->_flag = false;
        this->_isfinished = true;
        // if (this->_isCGI == true)
        // {
        //     freeEnv(this->env);
        //     remove(this->_path.c_str());
        // }
        this->_isCGI = false;
        cout << GREEN "=====>end<====\n" RESET;
    }
}

void Response::DELETE(string path)
{
    cout << RED "======DELETE=======" RESET << endl; 
    if (is_adir(path))
    {
        cout << RED "======DELETE DIRECTORY=======" RESET << endl; 
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
        cout << RED "======DELETE FILE=======" RESET << endl; 
        ifstream file;
        cout << "Pth: " << path << endl;
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

void Response::sendResponse(Request &request, int fdSocket)
{
    cout << BLUE"======================RESPONSE===========================\n" RESET;
    if (!this->_flag)
    {
        this->_fdSocket = fdSocket;
        this->_path = request.directives.requestedFile;
        this->_statusCode = request.getStatusCode();
        this->_path = request.directives.requestedFile;
        this->_method = request.getMethod();
        this->_target = request.directives.requestTarget;
        this->_isErrorCode = request.isErrorCode;
        this->_absPath = request.directives.requestedFile;
        // this->_isCGI = false;
    }
    // cout << "Target: " << this->_target << endl;
    // cout << "Is Error: " << this->_isErrorCode  << endl;
    cout << "Path: " << this->_path << endl;
    cout << "Method: " << this->_method << endl;
    if (this->_isErrorCode == true)
    {   if (!this->_flag)
            checkErrors(request);
        if (!this->_defaultError)
            GET(request);
    }
    // else if ((!this->_flag && request.directives.isCgiAllowed)
    //     && (this->_path.rfind(".php") != string::npos
    //     || this->_path.rfind(".py") != string::npos))
    // {
    //     ifstream file;
    //     file.open(this->_path.c_str(), ios::binary);
    //     cout << "CGI: " << this->_path << endl;
    //     if (file.is_open())
    //     {
    //         cout << GREEN"========CGI======" RESET << endl;
    //         file.close();
    //         if (this->_path.rfind(".php") != string::npos)
    //             this->_cgiPath = "/usr/bin/php-cgi";
    //         else
    //             this->_cgiPath = "/usr/bin/python3";
    //         CGI(request);
    //     }
    //     else
    //     {
    //         this->_isErrorCode = true;
    //         this->_statusCode = 404;
    //         checkErrors(request);
    //         if (!this->_defaultError)
    //             GET(request);
    //     }
    // }
    else if (this->_method == "GET" && !this->_isErrorCode)
    {
        if (!this->_flag)
            checks(request);
        if (this->_cgiAutoIndex)
            CGI(request);
        else
            GET(request);
    }
    else if (this->_method == "POST" && !this->_isErrorCode)
    {
        checkErrors(request);
        if (!this->_defaultError)
            GET(request);
    }
    else if (this->_method == "DELETE" && !this->_isErrorCode)
    {
        DELETE(this->_path);
        checkErrors(request);
        if (!this->_defaultError)
            GET(request);
    }
    cout << BLUE"======================RESPONSE===========================\n" RESET;
}

void Response::checks(Request &request)
{
    if (this->_target.empty())
        this->_target = "/";
    
    if (this->_statusCode == 301 || (is_adir(this->_path) && this->_target[this->_target.length() - 1] != '/'))
    {
        cout << RED"========REDIRECTION======"  << endl;
        cout << "Target: " << this->_target << RESET<< endl;
        if (this->_statusCode == 301)
            this->_path = request.directives.returnRedirect;
        else
        {
            this->_path = this->_target + "/";
            this->_statusCode = 301;
        }
        SendHeader();
        this->_isfinished = true;
    }
    else if (is_adir(this->_path) && !this->_flag)
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
    cout << "checkAutoInedx: " << request.directives.autoindex << endl;
        for (size_t i = 0; i < request.directives.indexs.size(); i++)
        {
            string index = this->_path + request._location->getIndexs()[i];
            cout << "index: " << index << endl;
            this->file.open(index.c_str(), ios::in | ios::binary);
            if (file.is_open())
            {
                // exit(0);
                this->_path = index;
                cout << "==============>INDEX Path: " << this->_path << endl;
                cout << "is auto index: " << request.directives.isCgiAllowed << endl;
                cout << "is auto find Cghi extention: " << this->_path.rfind(".php") << endl;

                if (request.directives.isCgiAllowed && (this->_path.rfind(".php") != string::npos || this->_path.rfind(".py") != string::npos))
                {
                    cout << "CGI with index\n";
                    this->_absPath = this->_path;
                    file.close();
                    if (this->_path.rfind(".php") != string::npos)
                        this->_cgiPath = "/usr/bin/php-cgi";
                    else
                        this->_cgiPath = "/usr/bin/python3";
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
    {
        cout << "tree_dir\n";
        tree_dir();

    }
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
        struct dirent *dp;
        string name;
        string filePath;
        string body = "<html><head></head><body><ul>";
        while ((dp = readdir(dir)))
        {
            name = dp->d_name;
            body += "<li><a href='"+ name  +"'>"  + name +"</a></li>";
        }
        this->_contentType = "text/html";
        stringstream ss;
        SendHeader();
        ss << hex << body.length();
        this->_body += ss.str() + "\r\n";
        this->_body += body + "\r\n";
        this->_body += "0\r\n\r\n";
        write(this->_fdSocket, this->_body.c_str(),  this->_body.length());
        this->_defaultError = true;
        this->_isfinished = true;
        this->_flag = false;
    }
}

string Response::getErrorPage(Request &request, int statusCode)
{
    cout << "getErrorPage\n";
    map<string, string>::iterator it;
    it = request.directives.errorPages.find(toSting(statusCode));
    if (it != request.directives.errorPages.end())
        return it->second;
    return "default";
}

void Response::checkErrors(Request &request)
{
    cout << RED "ERRORS HANDLER\n" RESET;
    cout << RED "Status code: "<< this->_statusCode << RESET << endl;

    this->_path = getErrorPage(request, this->_statusCode);
    cout << "error Path: " << this->_path << endl;
    if (!this->_flag)
        file.open(this->_path.c_str(), ios::in | ios::binary);
    if (!file.is_open() || this->_path == "default")
    {
        string error;
        stringstream ss;
        map<int, string>::iterator it;
        this->_contentType = "text/html";
        SendHeader();
        it = this->status.find(this->_statusCode);
        error = templateError(it->second);
        ss << hex << error.length();
        this->_body = ss.str() + "\r\n";
        this->_body += error + "\r\n";
        this->_body += "0\r\n\r\n";
        write(this->_fdSocket, this->_body.c_str(),  this->_body.length());
        this->_defaultError = true;
        this->_isfinished = true;
        cout << "END DEFAULT\n" << endl;
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
    map<int,string>::iterator it;
    it = this->status.find(this->_statusCode);
    this->_header = "HTTP/1.1 " + it->second +"\r\n";
    if(_statusCode == 301)
        this->_header += "Location: " + this->_path +"\r\n\r\n";
    else
    {
        if (this->_contentType.empty())
            this->_header += "Content-Type: text/html\r\n";
        else
            this->_header += "Content-Type: " + this->_contentType + "\r\n";
        this->_header += "Transfer-Encoding: chunked\r\n";
        if (this->_isCGI == true)
            this->_header += this->_cgiHeader;
        this->_header += "connection: close\r\n\r\n";
    }
    // cout << "Header: " << this->_header << endl;
    int n = write(this->_fdSocket, this->_header.c_str(), this->_header.length());
    if (n < -1)
        return;
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
    this->env = new char *[9]; 
    env[0] = strdup(("REQUEST_METHOD=" + this->_method).c_str());
    env[1] = strdup(("QUERY_STRING=" + req.directives.queryString).c_str());
    env[2] = strdup("REDIRECT_STATUS=200");
    env[3] = strdup(("PATH_INFO=" + this->_absPath).c_str());
    env[4] = strdup(("SCRIPT_FILENAME=" + this->_absPath).c_str());
    env[5] = strdup(("CONTENT_TYPE=" + req.directives.contentType).c_str());
    if (this->_method == "GET")
        env[6] = strdup("CONTENT_LENGTH=0");
    else
    {
        double size = fileSize(req.directives.cgiFileName);
        env[6] = strdup(("CONTENT_LENGTH=" + toSting(size)).c_str());
    }
    env[7] = strdup(("HTTP_COOKIE=" + req.directives.httpCookie).c_str());
    env[8] = NULL;
    return 1;
}

void Response::freeEnv(char **env)
{
    for (int i = 0; env[i]; i++)
        free(env[i]);
    delete[] env;
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
        this->_fdSocket = other._fdSocket;
        this->_statusCode = other._statusCode;
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
        this->env = other.env;
        this->_cgiAutoIndex = other._cgiAutoIndex;
        this->start = other.start;
        this->_randPath = other._randPath;
    }
    return *this;
}

bool Response::getIsFinished() const
{
    return this->_isfinished;
}

Response::~Response()
{
    cout << "Response destructor\n";
    // close(this->_fdSocket);
}