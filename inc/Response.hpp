#pragma once

// #include "Server.hpp"
#include "Location.hpp"
#include "Request.hpp"
#include <map>
#include <string>
#include <iostream>  
#include <fstream>
#include <signal.h>
#include <sys/stat.h>

#define BUFFERSIZE 1024
class Response
{
    private:
        // char _body1[BUFFERSIZE];
        int _fdSocket;
        int _statusCode;
        std::string _path;
        std::string _contentType;
        std::string _header;
        int _contentLength;
        std::string _body;
        std::ifstream file;
        bool _flag;
        std::stringstream statusString;
        std::map<std::string, std::string> mime;
        std::map<int, std::string> status;
    public:
        Response();
        ~Response();
        void GET(Request &request);
        void sendResponse(Request &request, int fdSocket);
    private:
        void SendHeader();
        void findeContentType();
        void saveStatus();
        int is_adir(std::string path);

        // void Delete(Request &request, Location &locations);
};