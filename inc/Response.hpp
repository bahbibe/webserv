#pragma once

// #include "Server.hpp"
#include "Location.hpp"
#include "Request.hpp"
#include <map>
#include <string>
#include <iostream>  
#include <fstream>
#define BUFFERSIZE 1024
class Response
{
    private:
        // char _body1[BUFFERSIZE];
        int _fdSocket;
        std::string _path;
        std::string _contentType;
        std::string _header;
        int _contentLength;
        std::string _body;
        std::stringstream statusString;
        std::map<std::string, std::string> mime;
    public:
        Response(Request &request, int fdSocket);
        void SendHeader(int contentLength);
        void findeContentType();
        void GET(Request &request);
        // void Delete(Request &request, Location &locations);
        ~Response();
};