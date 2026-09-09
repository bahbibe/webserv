#include "../../inc/Response.hpp"

double Response::fileSize(string path)
{
    struct stat metadata;
    stat(path.c_str(), &metadata);
    return metadata.st_size;
}

string Response::resolveCgiPath(Request &request) const
{
    string ext = (this->_path.rfind(".php") != string::npos) ? "php" : "py";
    map<string, string>::const_iterator it = request.directives.cgiPaths.find(ext);
    if (it != request.directives.cgiPaths.end())
        return it->second;
    return ext == "php" ? "/usr/bin/php-cgi" : "/usr/bin/python3";
}

int Response::getCgiStdoutFd() const
{
    return this->_cgiStdoutFd;
}

int Response::getCgiStdinFd() const
{
    return this->_cgiStdinFd;
}

void Response::closeCgiStdin(map<int, int> &cgiFdToClient)
{
    if (this->_cgiStdinFd != -1)
    {
        epoll_ctl(ep.epollFd, EPOLL_CTL_DEL, this->_cgiStdinFd, NULL);
        cgiFdToClient.erase(this->_cgiStdinFd);
        close(this->_cgiStdinFd);
        this->_cgiStdinFd = -1;
    }
    if (this->_cgiStdinFile.is_open())
        this->_cgiStdinFile.close();
}

void Response::closeCgiPipes(map<int, int> &cgiFdToClient)
{
    if (this->_cgiStdoutFd != -1)
    {
        epoll_ctl(ep.epollFd, EPOLL_CTL_DEL, this->_cgiStdoutFd, NULL);
        cgiFdToClient.erase(this->_cgiStdoutFd);
        close(this->_cgiStdoutFd);
        this->_cgiStdoutFd = -1;
    }
    closeCgiStdin(cgiFdToClient);
}

void Response::CGI(Request &req, map<int, int> &cgiFdToClient)
{
    if (this->_isCGI == false)
    {
        this->_isCGI = true;
        this->start = time(NULL);
        fillEnv(req);
        cout.flush();

        bool needsStdin = (this->_method != "GET" && this->_method != "HEAD");
        int outPipe[2];
        int inPipe[2] = {-1, -1};
        pipe(outPipe);
        if (needsStdin)
            pipe(inPipe);

        this->pid = fork();
        if (this->pid == 0)
        {
            close(outPipe[0]);
            dup2(outPipe[1], STDOUT_FILENO);
            close(outPipe[1]);
            if (needsStdin)
            {
                close(inPipe[1]);
                dup2(inPipe[0], STDIN_FILENO);
                close(inPipe[0]);
            }
            else
                close(STDIN_FILENO);
            const char *argv[] = {this->_cgiPath.c_str(), this->_absPath.c_str() ,NULL};
            vector<char *> envp;
            for (size_t i = 0; i < this->_cgiEnv.size(); i++)
                envp.push_back(const_cast<char *>(this->_cgiEnv[i].c_str()));
            envp.push_back(NULL);
            size_t slash = this->_absPath.rfind('/');
            if (slash != string::npos)
                chdir(this->_absPath.substr(0, slash).c_str());
            execve(argv[0], (char* const*)argv, envp.data());
            spdlog::error("execve: {}", strerror(errno));
            exit(127);
        }

        close(outPipe[1]);
        this->_cgiStdoutFd = outPipe[0];
        fcntl(this->_cgiStdoutFd, F_SETFL, O_NONBLOCK);
        ep.event.data.fd = this->_cgiStdoutFd;
        ep.event.events = EPOLLIN;
        epoll_ctl(ep.epollFd, EPOLL_CTL_ADD, this->_cgiStdoutFd, &ep.event);
        cgiFdToClient[this->_cgiStdoutFd] = this->_fdSocket;

        if (needsStdin)
        {
            close(inPipe[0]);
            this->_cgiStdinFd = inPipe[1];
            fcntl(this->_cgiStdinFd, F_SETFL, O_NONBLOCK);
            ep.event.data.fd = this->_cgiStdinFd;
            ep.event.events = EPOLLOUT;
            epoll_ctl(ep.epollFd, EPOLL_CTL_ADD, this->_cgiStdinFd, &ep.event);
            cgiFdToClient[this->_cgiStdinFd] = this->_fdSocket;
            this->_cgiStdinFile.open(req.directives.cgiFileName.c_str(), ios::in | ios::binary);
        }
        else
        {
            if (inPipe[0] != -1)
                close(inPipe[0]);
            if (inPipe[1] != -1)
                close(inPipe[1]);
        }
        return;
    }

    // Subsequent calls, driven by the client socket's own EPOLLOUT ticks:
    // reap/timeout-check the child, and flush whatever relayCgiOutput()
    // (driven by the CGI stdout pipe's own EPOLLIN ticks) has already
    // queued into _body.
    if (!this->_cgiReaped)
    {
        int status = 0;
        pid_t wPid = waitpid(this->pid, &status, WNOHANG);
        if (wPid > 0)
        {
            this->_cgiReaped = true;
            this->_cgiExitStatus = status;
        }
    }
    if (!this->_cgiTimedOut && CLOCKWORK(this->start) > CGI_TIMEOUT)
    {
        this->_cgiTimedOut = true;
        if (!this->_cgiReaped)
        {
            kill(this->pid, SIGKILL);
            waitpid(this->pid, 0, 0);
            this->_cgiReaped = true;
        }
        if (!this->_cgiHeaderParsed)
        {
            this->closeCgiPipes(cgiFdToClient);
            this->_cgiEnv.clear();
            remove(req.directives.cgiFileName.c_str());
            this->_isErrorCode = true;
            this->_statusCode = 504;
            checkErrors(req);
            return;
        }
        this->closeCgiPipes(cgiFdToClient);
        this->_body = "0\r\n\r\n";
        this->_bodyOffset = 0;
        this->_pendingBodyLen = 0;
        this->_cgiDone = true;
    }

    if (this->_isHead)
    {
        if (this->_cgiHeaderParsed)
        {
            this->_flag = false;
            this->_isfinished = true;
            this->closeCgiPipes(cgiFdToClient);
            this->_cgiEnv.clear();
            remove(req.directives.cgiFileName.c_str());
            this->_isCGI = false;
        }
        return;
    }

    if (this->_bodyOffset < this->_body.length())
    {
        if (!flushBody())
            return;
        this->_bytesSent += this->_pendingBodyLen;
        if (this->_cgiDone)
        {
            this->_flag = false;
            this->_isfinished = true;
            this->closeCgiPipes(cgiFdToClient);
            this->_cgiEnv.clear();
            remove(req.directives.cgiFileName.c_str());
            this->_isCGI = false;
        }
    }
}

bool Response::relayCgiOutput(Request &request, map<int, int> &cgiFdToClient)
{
    if (this->_bodyOffset < this->_body.length())
        return false; // backpressure: previous chunk not yet fully sent to the client

    char buf[4096];
    ssize_t n = read(this->_cgiStdoutFd, buf, sizeof(buf));
    if (n < 0)
        return false; // not ready / spurious, retry next tick - no errno inspection

    if (n > 0)
        this->_cgiOutBuf.append(buf, (size_t)n);

    if (!this->_cgiHeaderParsed)
    {
        if (n == 0)
        {
            // EOF before any header ever arrived - definite CGI error.
            this->closeCgiPipes(cgiFdToClient);
            this->_isErrorCode = true;
            this->_statusCode = this->_cgiTimedOut ? 504 : 500;
            checkErrors(request);
            return true;
        }
        size_t pos = this->_cgiOutBuf.find("\r\n\r\n");
        if (pos == string::npos)
            return false; // still accumulating header bytes
        this->_cgiHeader = this->_cgiOutBuf.substr(0, pos + 2);
        this->_cgiOutBuf.erase(0, pos + 4);
        this->_cgiHeaderParsed = true;
        this->_statusCode = 200;
        SendHeader();
        if (this->_cgiOutBuf.empty())
            return true; // header found, no body bytes yet - wait for the next readable tick
    }

    if (n == 0)
    {
        // EOF: CGI finished producing output - relay whatever's left and stop.
        epoll_ctl(ep.epollFd, EPOLL_CTL_DEL, this->_cgiStdoutFd, NULL);
        cgiFdToClient.erase(this->_cgiStdoutFd);
        close(this->_cgiStdoutFd);
        this->_cgiStdoutFd = -1;
        this->closeCgiStdin(cgiFdToClient); // CGI is done - stop feeding any leftover input

        string tail = this->_cgiOutBuf;
        this->_cgiOutBuf.clear();
        if (!tail.empty())
        {
            stringstream ss;
            ss << hex << tail.length();
            this->_body = ss.str() + "\r\n" + tail + "\r\n0\r\n\r\n";
        }
        else
            this->_body = "0\r\n\r\n";
        this->_pendingBodyLen = tail.length();
        this->_bodyOffset = 0;
        this->_cgiDone = true;
        return true;
    }

    if (this->_cgiOutBuf.empty())
        return true; // nothing new to relay this call
    stringstream ss;
    ss << hex << this->_cgiOutBuf.length();
    this->_body = ss.str() + "\r\n" + this->_cgiOutBuf + "\r\n";
    this->_pendingBodyLen = this->_cgiOutBuf.length();
    this->_bodyOffset = 0;
    this->_cgiOutBuf.clear();
    return true;
}

bool Response::flushCgiStdin(map<int, int> &cgiFdToClient)
{
    if (this->_cgiStdinChunk.empty())
    {
        char buf[4096];
        this->_cgiStdinFile.read(buf, sizeof(buf));
        streamsize got = this->_cgiStdinFile.gcount();
        if (got <= 0)
        {
            // fully sent - close our write end so the CGI sees EOF on its stdin.
            this->closeCgiStdin(cgiFdToClient);
            return true;
        }
        this->_cgiStdinChunk.assign(buf, (size_t)got);
        this->_cgiStdinChunkOffset = 0;
    }
    ssize_t n = write(this->_cgiStdinFd, this->_cgiStdinChunk.data() + this->_cgiStdinChunkOffset,
                       this->_cgiStdinChunk.size() - this->_cgiStdinChunkOffset);
    if (n <= 0)
        return false;
    this->_cgiStdinChunkOffset += (size_t)n;
    if (this->_cgiStdinChunkOffset >= this->_cgiStdinChunk.size())
        this->_cgiStdinChunk.clear();
    return true;
}

void Response::fillEnv(Request &req)
{
    string requestUri = req.directives.requestTarget;
    if (!req.directives.queryString.empty())
        requestUri += "?" + req.directives.queryString;

    this->_cgiEnv.push_back("REQUEST_METHOD=" + this->_method);
    this->_cgiEnv.push_back("QUERY_STRING=" + req.directives.queryString);
    this->_cgiEnv.push_back("REDIRECT_STATUS=200");
    this->_cgiEnv.push_back("PATH_INFO=");
    this->_cgiEnv.push_back("SCRIPT_FILENAME=" + this->_absPath);
    this->_cgiEnv.push_back("SCRIPT_NAME=" + req.directives.requestTarget);
    this->_cgiEnv.push_back("REQUEST_URI=" + requestUri);
    this->_cgiEnv.push_back("GATEWAY_INTERFACE=CGI/1.1");
    this->_cgiEnv.push_back("SERVER_PROTOCOL=" + req.getHttpVersion());
    this->_cgiEnv.push_back("SERVER_SOFTWARE=webserv/1.0");
    this->_cgiEnv.push_back("REMOTE_ADDR=" + req._clientIp);
    this->_cgiEnv.push_back("CONTENT_TYPE=" + req.directives.contentType);
    if (req.getServer())
    {
        this->_cgiEnv.push_back("SERVER_NAME=" + req.getServer()->getHost());
        this->_cgiEnv.push_back("SERVER_PORT=" + req.getServer()->getPort());
    }
    if (this->_method == "GET" || this->_method == "HEAD")
        this->_cgiEnv.push_back("CONTENT_LENGTH=0");
    else
    {
        double size = fileSize(req.directives.cgiFileName);
        this->_cgiEnv.push_back("CONTENT_LENGTH=" + toSting(size));
    }
    map<string, string> headers = req.getHeaders();
    for (map<string, string>::iterator it = headers.begin(); it != headers.end(); ++it)
    {
        string name = it->first;
        if (name == "content-type" || name == "content-length")
            continue;
        string envName = "HTTP_";
        for (size_t i = 0; i < name.length(); i++)
            envName += (name[i] == '-') ? '_' : (char)toupper((unsigned char)name[i]);
        this->_cgiEnv.push_back(envName + "=" + it->second);
    }
}
