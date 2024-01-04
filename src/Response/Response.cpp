#include "../../inc/Response.hpp"

Response::Response():_fdSocket(0), _statusCode(0),_isfinished(0),_flag(false)
{
}


void Response::sendResponse(Request &request, int fdSocket)
{
    cout << BLUE"======================RESPONSE===========================\n" RESET;
    this->_fdSocket = fdSocket;
    this->_path = request.getRequestTarget();
    this->_statusCode = request.getStatusCode();
    this->_path = request.getFileFullPath();
    cout << "Path: " << this->_path << endl;

    cout << "Flag: " << this->_flag << endl;
    cout << "Method: " << request.getMethod() << endl;
    if (is_adir(this->_path)&& !this->_flag)
        checkAutoInedx(request);
    if (request.isErrorCode && !this->_flag)
        checkErrors(request);
    else if (!this->_flag)
    {
        //!openig file and check exist.
        cout << "request.getAutoIndex(): " << request.getAutoIndex() << endl;

        this->file.open(_path.c_str(), ios::in | ios::binary);
        if (!file.good())
        {
            request.isErrorCode = 1;
            this->_path = "./WWW/err/404.html";
            this->_statusCode = 404;
            this->file.open(_path.c_str(), ios::in | ios::binary);
        }
        saveStatus();
        findeContentType();
        SendHeader();
        this->_flag = true;
    }

    if (request.getMethod() == "GET")
    {
        GET(request);
    }
    else if (request.getMethod() == "DELETE")
    {
        // DELETE(request);
        cout << "DELETE\n";
    }
    cout << BLUE"======================RESPONSE===========================\n" RESET;

}


void Response::checkAutoInedx(Request &request)
{
    cout << "checkAutoInedx: " << request.getAutoIndex() << endl;
    if (request.getAutoIndex())
    {
        for (size_t i = 0; i < request._location->getIndexs().size(); i++)
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
        cout << "tree file is loading ^^\n";
        exit(1);
    }
    else
    {
        request.isErrorCode = 1;
        this->_path = "./WWW/err/403.html";
        this->_statusCode = 403;
    }

}

void Response::checkErrors(Request &request)
{
    cout << RED "ERRORS HSNDLER\n" RESET;
    file.open(this->_path.c_str(), ios::in | ios::binary);
    if (!file.good())
    {
        request.isErrorCode = 1;
        this->_path = "./WWW/err/404.html";
        this->_statusCode = 404;
        this->file.open(_path.c_str(), ios::in | ios::binary);
    }
    this->_flag = true;
    saveStatus();
    findeContentType();
    SendHeader();
}

int Response::is_adir(string path)
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
    perror("ERROR");
    if (n < -1)
        return;
}

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
        this->_flag = false;
        this->_isfinished = 1;
        write(this->_fdSocket, this->_body.c_str(),   this->_body.length());
        file.close();
        close(this->_fdSocket);
        cout << GREEN "=====>end<====\n" RESET;
    }
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
int Response::getIsFinished() const
{
    return this->_isfinished;
}
Response::~Response()
{
    cout << "Response destructor\n";
    // close(this->_fdSocket);
}