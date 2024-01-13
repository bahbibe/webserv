#include "../../inc/Response.hpp"

Response::Response():_flag(false),_isfinished(false),_defaultError(false),_isErrorCode(false), _fdSocket(0), _statusCode(0)
{
    saveStatus();
}
// Response::Response(Request request, int fdSocket):_flag(false),_isfinished(false),_defaultError(false)
// {
//     this->_fdSocket = fdSocket;
//     this->_path = request.getRequestTarget();
//     this->_statusCode = request.getStatusCode();
//     this->_path = request.directives.requestedFile;
//     this->_method = request.getMethod();
//     saveStatus();
// }

//! redirece directory if does't end with "/"
//! redirection 

void Response::GET(Request &request)
{
    (void)request;
    signal(SIGPIPE, SIG_IGN);
    char _body1[BUFFERSIZE] = {0};
    file.read(_body1, 1023);
    if (file.gcount() > 0)
    {
        cout << YELLOW "====>start reading<====\n" RESET;
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
        this->_path = request.getRequestTarget();
        this->_statusCode = request.getStatusCode();
        this->_path = request.directives.requestedFile;
        this->_method = request.getMethod();
        this->_isErrorCode = request.isErrorCode;
    }
    cout << "Method: " << this->_method << endl;
    cout << "Is Error: " << this->_isErrorCode  << endl;
    cout << "Path: " << this->_path << endl;
    if (this->_isErrorCode == true)
    {   if (!this->_flag)
            checkErrors(request);
        if (!this->_defaultError)
            GET(request);
    }
    else if (this->_statusCode == 301 || (is_adir(this->_path) && this->_path[this->_path.length() - 1] != '/'))
    {
        cout << RED"========REDIRECTION======" RESET << endl;
        if (this->_statusCode == 301)
        {
            this->_header += "HTTP/1.1 301 Moved Permanently\r\nLocation:" + request.directives.returnRedirect + "\r\n\r\n";
        }
        else
        {
            cout << BLUE "directory redirection\n" RESET;
            // exit(10);
            this->_path += "/";

            cout << "PATH REDIRECTION : " << this->_path ;
            this->_header += "HTTP/1.1 301 Moved Permanently\r\nLocation:" + this->_path + "\r\n\r\n";
            // cout << _path << endl;
        }
        write(this->_fdSocket, this->_header.c_str(), this->_header.length());
        this->_isfinished = true;
    }
    else if (this->_method == "GET" && !this->_isErrorCode)
    {
        if (!this->_flag)
            checks(request);
        if (!this->_defaultError)
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
            findeContentType();
            SendHeader();
            this->_flag = true;
        }
    }
}
void Response::checkAutoInedx(Request &request)
{
    cout << "checkAutoInedx: " << request.directives.autoindex << endl;
    if (request.directives.autoindex)
    {
        for (size_t i = 0; i < request.directives.indexs.size(); i++)
        {
            string index = this->_path + request._location->getIndexs()[i];
            cout << "index: " << index << endl;
            this->file.open(index.c_str(), ios::in | ios::binary);
            if (file.good())
            {
                this->_path = index;
                this->_statusCode = 200;
                this->_flag = true;
                saveStatus();
                findeContentType();
                SendHeader();
                return;
            }   
        }
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
        findeContentType();
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
    this->status[500] = "500 Internal Server Error";
    this->status[501] = "501 Not Implemented";
}

void Response::SendHeader() 
{
    map<int,string>::iterator it;
    it = this->status.find(this->_statusCode);
    this->_header = "HTTP/1.1 " + it->second +"\r\n";
    this->_header += "Content-Type: " + this->_contentType + "\r\n";
    this->_header += "Transfer-Encoding: chunked\r\n";
    this->_header += "connection: close\r\n\r\n";
    int n = write(this->_fdSocket, this->_header.c_str(), this->_header.length());
    // perror("ERROR");
    if (n < -1)
        return;
}

string Response::templateError(string errorType)
{
    string errorBody;

    errorBody = "<html><head><title>"+ errorType +"</title></head>";
    errorBody += "<body><center><h1>"+errorType+ "</h1></center><hr><center>M0BLACK</center></body>";
    return errorBody;
}


void Response::findeContentType()
{
    ifstream file("./conf/mime.conf");
    int idex= this->_path.rfind(".");
    string extention = this->_path.substr(idex + 1);

    if (file.good())
    {
        stringstream ss;
        string line;
        string key;
        string val; 
        while (getline(file, line))
        {
            ss << line;
            ss >> val;
            ss >> key;
            this->mime[key] = val;
            ss.clear();
        }
        map<string, string>::iterator it;
        it = this->mime.find(extention);
        if (it != this->mime.end())
            this->_contentType = it->second;
    }
}

string Response::toSting(int &mun)
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
        this->_fdSocket = other._fdSocket;
        this->_statusCode = other._statusCode;
        this->_path = other._path;
        this->_contentType = other._contentType;
        this->_header = other._header;
        this->_contentLength = other._contentLength;
        this->_body = other._body;
        // this->file = other.file;
        this->_flag = other._flag;
        // this->statusString = other.statusString;
        this->mime = other.mime;
        this->status = other.status;
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