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
#include <unistd.h>
#include <dirent.h>

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

        string _method;
        string _path;
        string _contentType;
        string _header;
        string _body;

        ifstream file;
        stringstream statusString;

        map<string, string> mime;
        map<int, string> status;
        

    public:
        Response();
        // Response(Request request, int fdSocket);
        ~Response();
        void sendResponse(Request &request, int fdSocket);
        Response(const Response &other);
        Response &operator=(const Response &other);
        bool getIsFinished() const;
        void GET(Request &request);
        void DELETE(string path);
    private:
        void SendHeader();
        void findeContentType();
        void saveStatus();
        int is_adir(string &path);
        void checkAutoInedx(Request &request);
        void checkErrors(Request &request);
        void tree_dir();
        string toSting(int &mun);
        string getErrorPage(Request &request, int statusCode);
        string templateError(string errorType);
        void checks(Request &request);
};


//! TO DO
//! - fix pathes that end with "/"
//! - Delete method
//! - clearing code