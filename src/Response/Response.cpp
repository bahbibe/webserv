#include "../../inc/Response.hpp"

Response::Response():_fdSocket(0), flag(false)
{
    // this->_path = request.getRequestTarget();
    // std::cout << request.getRequestTarget() << "\n";
    // if (this->_path == "/")
    //     this->_path = "./WWW/test.txt";
    // findeContentType();
    // if (request.isErrorCode)
    // {
    //     std::cout << "Error\n";
    //     GET(request);
    //     // SendHeader(request._socketFd);
    // }
    // else if (request.getMethod() == "GET")
    // {
    //     GET(request);
    //     std::cout << "GET\n";
    // }
}

void Response::sendResponse(Request &request, int fdSocket)
{
    this->_fdSocket = fdSocket;
      this->_path = request.getRequestTarget();
    std::cout << request.getRequestTarget() << "\n";
    if (this->_path == "/")
        this->_path = "./WWW/test.txt";
    findeContentType();
    if (request.isErrorCode)
    {
        std::cout << "Error\n";
        GET(request);
        // SendHeader(request._socketFd);
    }
    else if (request.getMethod() == "GET")
    {
        GET(request);
        // std::cout << "GET\n";
    }
}
void Response::SendHeader(int contentLength) {
    (void)contentLength;
    // this->statusString << contentLength;
    // std::string len  = statusString.str();

    this->_header = "HTTP/1.1 200 OK\r\n";
    this->_header += "Content-Type: " + this->_contentType + "\r\n";
    // this->_header += "Content-Type: text/html\r\n";
    this->_header += "Transfer-Encoding: chunked\r\n";
    // this->_header += "Content-Length: " + len + "\r\n";
    this->_header += "connection: close\r\n\r\n";
    write(this->_fdSocket, this->_header.c_str(), this->_header.length());

}

void Response::GET(Request &request)
{
    if (request.isErrorCode)
    {
        std::stringstream ss;
        std::string status;
        ss << request.getStatusCode();
        status = ss.str();
        this->_path = "./err/" + status + ".html";
        // std::ifstream file(this->_path.c_str(), std::ios::binary);
        std::ifstream file(this->_path.c_str());
        if (file.is_open())
        {
            // file.read(this->_body, BUFFERSIZE);
            std::stringstream buffer;
            buffer << file.rdbuf();
            this->_body = buffer.str();
            SendHeader(this->_body.length());
            // file.read(this->_body, 1024);
            // this->_header += this->_body;
            // std::cout << _header << "\n";
            // write(this->_fdSocket, this->_body.c_str(),  this->_body.length());
            // write(request._socketFd, this->_body ,  1024);
        }
        else
            std::cout << "File not found!!!.\n";
    }
    else
    {
        char _body1[BUFFERSIZE] = {0};
         if (!this->flag)
        {
            // this->file(this->_path.c_str(), std::ios::binary);
            std::cout << "opne file \n";
             this->file.open(_path.c_str(), std::ios::binary);
             SendHeader(_body.length());
             this->flag = true;

        }
            file.read(_body1, 5);
            // std::cout << this->_body1;
            // file.read(this->_body1, 5);
            // std::cout << this->_body1;
            if (file.gcount() > 0)
            {
                
                std::cout << "cout: " << file.gcount() << "\n";
                this->_body = "5\r\n";
                this->_body += _body1;
                this->_body += "\r\n";
                // std::cout << "body1:"<< this->_body1 << "\n";
                // std::cout << "body: " << this->_body << "\n";
                write(this->_fdSocket, this->_body.c_str(),  this->_body.length());
                // std::cout << "====>1\n";


            }
            else if (file.gcount() == 0)
            {
                std::cout << "======>2\n";
                this->_body = "0\r\n\r\n";
                this->flag = false;
                write(this->_fdSocket, this->_body.c_str(),   this->_body.length());
                close(this->_fdSocket);
            }
            // this->_body += "0\r\n";
            // std::cout << "this is test file: \n"<< this->_body << "\n";

        // }
        // else
        //     std::cout << "File not found!!!.\n";
    }

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
        {
            std::cout << "Error::Type Not foud...!!!\n";
        }
        // std::cout << this->_contentType;
        // for (it = this->mime.begin(); it != this->mime.end(); it++)
        std::cout << "content-type: " << this->_contentType << "\n";
        
        
    }
    else
        std::cout << "file not found!!!\n";

}
Response::~Response()
{
    std::cout << "Response destructor\n";
}