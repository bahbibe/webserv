#include "../../inc/Response.hpp"

Response::Response():_fdSocket(0), flag(false){}

void Response::sendResponse(Request &request, int fdSocket)
{
    this->_fdSocket = fdSocket;
      this->_path = request.getRequestTarget();
    std::cout << request.getRequestTarget() << "\n";
    if (this->_path == "/")
        this->_path = "./WWW/400.html";
    findeContentType();
    if (request.isErrorCode)
    {
        std::cout << "Error\n";
        GET(request);
    }
    else if (request.getMethod() == "GET")
        GET(request);
}
void Response::SendHeader() 
{
    this->_header = "HTTP/1.1 200 OK\r\n";
    this->_header += "Content-Type: " + this->_contentType + "\r\n";
    this->_header += "Transfer-Encoding: chunked\r\n";
    this->_header += "connection: close\r\n\r\n";

    write(this->_fdSocket, this->_header.c_str(), this->_header.length());

}

void Response::GET(Request &request)
{
    (void)request;
    // if (request.isErrorCode)
    // {
    //     std::stringstream ss;
    //     std::string status;
    //     ss << request.getStatusCode();
    //     status = ss.str();
    //     this->_path = "./err/" + status + ".html";
    //     // std::ifstream file(this->_path.c_str(), std::ios::binary);
    //     std::ifstream file(this->_path.c_str());
    //     if (file.is_open())
    //     {
    //         // file.read(this->_body, BUFFERSIZE);
    //         std::stringstream buffer;
    //         buffer << file.rdbuf();
    //         this->_body = buffer.str();
    //         SendHeader(this->_body.length());
    //         // file.read(this->_body, 1024);
    //         // this->_header += this->_body;
    //         // std::cout << _header << "\n";
    //         // write(this->_fdSocket, this->_body.c_str(),  this->_body.length());
    //         // write(request._socketFd, this->_body ,  1024);
    //     }
    //     else
    //         std::cout << "File not found!!!.\n";
    // }
    // else
    // {
        char _body1[BUFFERSIZE] = {0};
        if (!this->flag)
        {
             this->file.open(_path.c_str(), std::ios::in | std::ios::binary);
             SendHeader();
             this->flag = true;
        }
        if (this->file.is_open())
        {
            file.read(_body1, 1023);
            if (file.gcount() > 0)
            {
                std::cout << "=====>file.gcount(): " << file.gcount() << "\n";
                std::cout << "=====>reading\n";
                stringstream ss;
                ss << std::hex << file.gcount();
                std::cout << "cout: " << ss.str() << "\n";
                this->_body = ss.str() + "\r\n";
                this->_body += _body1;
                this->_body += "\r\n";
                std::cout << "Body1: " << _body1 << "\n";
                std::cout << "++++_body: "<< this->_body << "\n";
                std::cout << "++++"<< this->_body.length() << "\n";
                int n = write(this->_fdSocket, this->_body.c_str(),  this->_body.length());
                if (n == -1)
                    std::cout << "write failed...!!!!\n";
            }
            else if (file.gcount() == 0)
            {
                std::cout << "=====>end\n";
                this->_body = "0\r\n\r\n";
                this->flag = false;
                file.close();
                write(this->_fdSocket, this->_body.c_str(),   this->_body.length());
                close(this->_fdSocket);
            }
        }
        else
            std::cout << "File not found!!!.\n";
    // }

}

void Response::findeContentType()
{
    std::ifstream file("./conf/mime.conf");
    std::cout << "path: "<< this->_path << "\n";
    int idex= this->_path.rfind(".");
    std::string extention = this->_path.substr(idex + 1);
    std::cout << extention << "\n";

    if (file.is_open())
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
}