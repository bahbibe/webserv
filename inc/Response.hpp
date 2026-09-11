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
#include <cerrno>
#include <filesystem>
#include <system_error>

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
        bool _isHead;
        bool _keepAlive;
        bool _deleteDone;

        int _fdSocket;
        int _statusCode;
        size_t _bytesSent;
        size_t _headerOffset;
        size_t _bodyOffset;
        size_t _pendingBodyLen;


        string _method;
        string _path;
        string _contentType;
        string _header;
        string _body;
        string _target;
        string _cgiPath;
        string _absPath;
        string _cgiHeader;
        time_t start;

        vector<string> _cgiEnv;

        int _cgiStdoutFd;
        int _cgiStdinFd;
        bool _cgiHeaderParsed;
        bool _cgiReaped;
        bool _cgiTimedOut;
        bool _cgiDone;
        int _cgiExitStatus;
        string _cgiOutBuf;
        string _cgiStdinChunk;
        size_t _cgiStdinChunkOffset;
        ifstream _cgiStdinFile;

        ifstream file;
        stringstream statusString;

        map<string, string> mime;
        map<int, string> status;
        

    public:
        Response();
        void sendResponse(Request &request, int fdSocket, map<int, int> &cgiFdToClient);
        Response(const Response &other);
        Response &operator=(const Response &other);
        bool getIsFinished() const;
        bool getKeepAlive() const;
        size_t getBytesSent() const;
        int getStatusCode() const;
        void GET();
        void DELETE(string path);
        pid_t pid;
        bool _isCGI;

        // CGI pipe I/O - driven by epoll events on the pipe fds
        // themselves (see Webserver::start()'s _cgiFdToClient routing),
        // not by the client socket's events like everything above.
        int getCgiStdoutFd() const;
        int getCgiStdinFd() const;
        bool relayCgiOutput(Request &request, map<int, int> &cgiFdToClient);
        bool flushCgiStdin(map<int, int> &cgiFdToClient);
        void closeCgiStdin(map<int, int> &cgiFdToClient);
        void closeCgiPipes(map<int, int> &cgiFdToClient);

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
        string resolveCgiPath(Request &request) const;
        bool flushHeader();
        bool flushBody();
        bool headerSent() const;
        void CGI(Request &req, map<int, int> &cgiFdToClient);
        void fillEnv(Request &req);
        double fileSize(string path);


};
