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
using namespace std;
class Response
{
    private:
        // char _body1[BUFFERSIZE];
        bool _flag;
        bool _isfinished;
        bool _defaultError;

        int _fdSocket;
        int _statusCode;
        int _contentLength;

        std::string _path;
        std::string _contentType;
        std::string _header;
        std::string _body;

        std::ifstream file;
        std::stringstream statusString;

        std::map<std::string, std::string> mime;
        std::map<int, std::string> status;

    public:
        Response();
        ~Response();
        void sendResponse(Request &request, int fdSocket);
        Response(const Response &other);
        Response &operator=(const Response &other);
        bool getIsFinished() const;
    private:
        void GET(Request &request);
        void SendHeader();
        void findeContentType();
        void saveStatus();
        int is_adir(std::string path);
        void checkAutoInedx(Request &request);
        void checkErrors(Request &request);
        
        string toSting(int mun);
        string getErrorPage(Request &request, int statusCode);
        // void Delete(Request &request, Location &locations);
};
