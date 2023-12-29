#include "../../inc/Response.hpp"

Response::Response():_fdSocket(0), _statusCode(0),_flag(false)
{
    std::cout << "Flag constructor: " << this->_flag << std::endl;
}

void Response::sendResponse(Request &request, int fdSocket)
{
    std::cout << BLUE"======================RESPONSE===========================\n" RESET;
    this->_fdSocket = fdSocket;
    this->_path = request.getRequestTarget();
    std::cout << "PATHHH:"<< request.getRequestTarget() << std::endl;
    this->_statusCode = request.getStatusCode();
    if (this->_path == "/")
        this->_path = "./WWW";
    if (is_adir(this->_path))
    {
        this->_path = "./WWW/err/501.html";
        std::cout << "Is Directory :).\n";
    }
    else
        std::cout << "Not Directory :).\n";
    //!befor opeing a file, you have to check if is a directory.  
    //!you have to costmize you header error.
    std::cout << "Flag: " << this->_flag << std::endl;
    if (!this->_flag)
    {
        //!openig file and check exist.
        this->file.open(_path.c_str(), std::ios::in | std::ios::binary);
        if (!file.good())
        {
            request.isErrorCode = 1;
            this->_path = "./WWW/err/404.html";
            this->_statusCode = 404;
            this->file.open(_path.c_str(), std::ios::in | std::ios::binary);
        }
        saveStatus();
        findeContentType();
        SendHeader();
        this->_flag = true;
    }

    if (request.isErrorCode)
    {
        std::cout << "Error\n";
        GET(request);
    }
    else if (request.getMethod() == "GET")
    {
        GET(request);
    }
    std::cout << BLUE"======================RESPONSE===========================\n" RESET;

}
int Response::is_adir(std::string path)
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
    std::map<int,std::string>::iterator it;
    it = this->status.find(this->_statusCode);
    this->_header = "HTTP/1.1 " + it->second +"\r\n";
    // this->_header = "HTTP/1.1 200 OK\r\n";
    this->_header += "Content-Type: " + this->_contentType + "\r\n";
    this->_header += "Transfer-Encoding: chunked\r\n";
    this->_header += "connection: close\r\n\r\n";
    int n = write(this->_fdSocket, this->_header.c_str(), this->_header.length());
    perror("ERROR");
    if (n < -1)
    {
        return;
    }
}

void Response::GET(Request &request)
{
    (void)request;
    signal(SIGPIPE, SIG_IGN);
    char _body1[BUFFERSIZE] = {0};
    if (this->file.is_open())
    {
        file.read(_body1, 1023);
        std::cout << "****************Start reading\n";
        if (file.gcount() > 0)
        {
            stringstream ss;
            ss << std::hex << file.gcount();
            this->_body = ss.str() + "\r\n";
            this->_body.append(_body1, file.gcount());
            this->_body += "\r\n";
            int n = write(this->_fdSocket, this->_body.c_str(),  this->_body.length());
            perror("ERROR");
            if (n == -1)
                std::cout << "write failed...!!!!\n";
        }
        else if (file.gcount() == 0)
        {
            this->_body = "0\r\n\r\n";
            // this->_body = "\r\n";
            this->_flag = false;
            file.close();
            std::cout << "=====>end\n";
            write(this->_fdSocket, this->_body.c_str(),   this->_body.length());
            close(this->_fdSocket);
        }
    }
    else
        std::cout << "File not found!!!.\n";
}

void Response::findeContentType()
{
    std::ifstream file("./conf/mime.conf");
    int idex= this->_path.rfind(".");
    std::string extention = this->_path.substr(idex + 1);
    std::cout << extention << "\n";

    if (this->_flag)
    {
        std::stringstream ss;
        std::string line;
        std::string key;
        std::string val; 
        while (getline(file, line))
        {
            ss << line;
            ss >> val;
            ss >> key;
            this->mime[key] = val;
            ss.clear();
        }
        std::map<std::string, std::string>::iterator it;
        it = this->mime.find(extention);
        if (it != this->mime.end())
            this->_contentType = it->second;
        else
            std::cout << "Error::Type Not foud...!!!\n";
        std::cout << "content-type: " << this->_contentType << "\n";
    }
    else
        std::cout << "file not found!!!\n";

}
Response::~Response()
{
    std::cout << "Response destructor\n";
    // close(this->_fdSocket);
}