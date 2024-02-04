#pragma once

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
#include <sys/types.h>
#include <sys/wait.h>

#define BUFFERSIZE 1024
using namespace std;
class Response
{
    private:
        bool _flag;
        bool _isfinished;
        bool _defaultError;
        bool _isErrorCode;
        bool _cgiAutoIndex;

        int _fdSocket;
        int _statusCode;
        int fd[2];


        string _method;
        string _path;
        string _contentType;
        string _header;
        string _body;
        string _target;
        string _cgiPath;
        string _absPath;
        string _cgiHeader;
        string _randPath;
        clock_t start;

        char **env;

        ifstream file;
        stringstream statusString;

        map<string, string> mime;
        map<int, string> status;
        

    public:
        Response();
        ~Response();
        void sendResponse(Request &request, int fdSocket);
        Response(const Response &other);
        Response &operator=(const Response &other);
        bool getIsFinished() const;
        void GET(Request &request);
        void DELETE(string path);
        pid_t pid;
        bool _isCGI;

    private:
        void initVars(Request &request, int fdSocket);
        void SendHeader();
        void findeContentType(Request &req);
        void saveStatus();
        int is_adir(string &path);
        void checkAutoInedx(Request &request);
        void checkErrors(Request &request);
        void tree_dir();
        string toSting(long long mun);
        string getErrorPage(Request &request, int statusCode);
        string templateError(string errorType);
        void checks(Request &request);
        void CGI(Request &req);
        int fillEnv(Request &req);
        double fileSize(string path);
        void freeEnv(char **env);


};
