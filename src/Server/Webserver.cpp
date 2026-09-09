#include "../../inc/webserv.hpp"
#include "../../inc/Server.hpp"
#include "../../inc/Request.hpp"
#include "../../inc/Response.hpp"

Webserver::Webserver()
{
    if ((ep.epollFd = epoll_create(1)) == -1)
        throw WebservException(ERR "Failed to create epoll");
    _accessLog.open(accessLogPath.c_str(), ios::app);
    if (!_accessLog.is_open())
        spdlog::warn("Unable to open access log at {}, continuing without it", accessLogPath);
}

void Webserver::reopenAccessLog()
{
    if (_accessLog.is_open())
        _accessLog.close();
    _accessLog.clear();
    _accessLog.open(accessLogPath.c_str(), ios::app);
    if (!_accessLog.is_open())
        spdlog::warn("Unable to reopen access log at {}, continuing without it", accessLogPath);
}


Server &Webserver::operator[](size_t index)
{
    return _servers[index];
}

Webserver::~Webserver()
{
    
}

void Webserver::newConnection(map<int, Request> &req, Server &server)
{
    int clientSock;
    struct sockaddr_storage clientAddr;
    socklen_t addrLen = sizeof(clientAddr);
    if ((clientSock = accept(server.getSocket(), (struct sockaddr *)&clientAddr, &addrLen)) == -1)
        throw WebservException(ERR "Accept failed");
    if (req.size() >= MAX_CONNECTIONS)
    {
        close(clientSock);
        return;
    }
    if (fcntl(clientSock, F_SETFL, O_NONBLOCK) == -1)
    {
        close(clientSock);
        throw WebservException(ERR "Failed to set client socket non-blocking");
    }
    ep.event.data.fd = clientSock;
    ep.event.events = EPOLLIN | EPOLLHUP | EPOLLRDHUP | EPOLLERR;
    if (epoll_ctl(ep.epollFd, EPOLL_CTL_ADD, clientSock, &ep.event))
    {
        close(clientSock);
        throw WebservException(ERR "Failed to add client to epoll");
    }
    req.insert(make_pair(clientSock, Request(&server, clientSock, _servers)));
    req[clientSock]._start = time(NULL);
    gettimeofday(&req[clientSock]._startTv, NULL);
    char ipStr[INET6_ADDRSTRLEN];
    void *addrPtr = (clientAddr.ss_family == AF_INET6)
        ? (void *)&((struct sockaddr_in6 *)&clientAddr)->sin6_addr
        : (void *)&((struct sockaddr_in *)&clientAddr)->sin_addr;
    if (inet_ntop(clientAddr.ss_family, addrPtr, ipStr, sizeof(ipStr)))
        req[clientSock]._clientIp = ipStr;
}

void Webserver::logAccess(Request &req, Response *resp)
{
    if (!_accessLog.is_open())
        return;
    time_t now = time(NULL);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    struct timeval nowTv;
    gettimeofday(&nowTv, NULL);
    long durationMs = (nowTv.tv_sec - req._startTv.tv_sec) * 1000
                     + (nowTv.tv_usec - req._startTv.tv_usec) / 1000;

    string clientIp = req._clientIp.empty() ? "-" : req._clientIp;
    string method = req.getMethod().empty() ? "-" : req.getMethod();
    string target = !req.directives.requestTarget.empty() ? req.directives.requestTarget
                    : !req.getRequestTarget().empty() ? req.getRequestTarget() : "-";
    if (!req.directives.queryString.empty())
        target += "?" + req.directives.queryString;

    string status = "-";
    size_t bytes = 0;
    if (req.getIsRequestFinished())
    {
        stringstream ss;
        ss << (resp ? resp->getStatusCode() : req.getStatusCode());
        status = ss.str();
        if (resp)
            bytes = resp->getBytesSent();
    }
    _accessLog << "[" << timestamp << "] " << clientIp << " " << method << " " << target
               << " " << status << " " << bytes << " " << durationMs << "ms\n";
    _accessLog.flush();
}

void Webserver::closeConnection(map<int, Request> &req, map<int, Response> &resp, int sock)
{
    map<int, Response>::iterator respIt = resp.find(sock);
    if (respIt != resp.end() && respIt->second._isCGI)
    {
        kill(respIt->second.pid, SIGKILL);
        waitpid(respIt->second.pid, 0, 0);
        respIt->second.closeCgiPipes(_cgiFdToClient);
    }
    logAccess(req[sock], respIt != resp.end() ? &respIt->second : NULL);
    epoll_ctl(ep.epollFd, EPOLL_CTL_DEL, sock, NULL);
    req.erase(sock);
    resp.erase(sock);
    close(sock);
}

void Webserver::safeCloseConnection(int sock)
{
    try
    {
        closeConnection(_req, _resp, sock);
    }
    catch (...)
    {
        map<int, Response>::iterator respIt = _resp.find(sock);
        if (respIt != _resp.end())
        {
            if (respIt->second._isCGI)
            {
                kill(respIt->second.pid, SIGKILL);
                waitpid(respIt->second.pid, 0, 0);
            }
            respIt->second.closeCgiPipes(_cgiFdToClient);
        }
        epoll_ctl(ep.epollFd, EPOLL_CTL_DEL, sock, NULL);
        close(sock);
        _req.erase(sock);
        _resp.erase(sock);
    }
}

bool Webserver::matchServer(map<int, Request> &req, int sock)
{
    for (size_t j = 0; j < _servers.size(); j++)
    {
        if (sock == _servers[j].getSocket())
        {
            newConnection(req, _servers[j]);
            return true;
        }
    }
    return false;
}

void Webserver::stopListening()
{
    for (map<string, UniqueFd>::iterator it = socketMap.begin(); it != socketMap.end(); ++it)
        epoll_ctl(ep.epollFd, EPOLL_CTL_DEL, it->second.get(), NULL);
    socketMap.clear();
}

void Webserver::start()
{
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, handleShutdownSignal);
    signal(SIGTERM, handleShutdownSignal);
    signal(SIGHUP, handleReopenLogSignal);
    time_t shutdownStarted = 0;
    while (1)
    {
        if (g_reopenLog)
        {
            g_reopenLog = 0;
            reopenAccessLog();
        }
        if (g_shutdown && shutdownStarted == 0)
        {
            shutdownStarted = time(NULL);
            stopListening();
            spdlog::info("Shutting down, waiting for {} in-flight connection(s)...", _req.size());
        }
        if (g_shutdown && _req.empty())
            break;
        if (g_shutdown && shutdownStarted && CLOCKWORK(shutdownStarted) > SHUTDOWN_GRACE)
        {
            spdlog::warn("Shutdown grace period elapsed, closing {} remaining connection(s).", _req.size());
            break;
        }
        int evCount = epoll_wait(ep.epollFd, ep.events, MAX_EVENTS, 1000);
        if (evCount == -1)
            continue;
        for (int i = 0; i < evCount; i++)
        {
            int fd = ep.events[i].data.fd;
            try
            {
                if (matchServer(_req, fd))
                    continue;

                map<int, int>::iterator cgiIt = _cgiFdToClient.find(fd);
                if (cgiIt != _cgiFdToClient.end())
                {
                    int clientFd = cgiIt->second;
                    map<int, Response>::iterator respIt = _resp.find(clientFd);
                    if (respIt == _resp.end())
                    {
                        // Orphaned pipe fd (shouldn't normally happen) - drop it defensively.
                        epoll_ctl(ep.epollFd, EPOLL_CTL_DEL, fd, NULL);
                        close(fd);
                        _cgiFdToClient.erase(fd);
                        continue;
                    }
                    if (fd == respIt->second.getCgiStdoutFd())
                        respIt->second.relayCgiOutput(_req[clientFd], _cgiFdToClient);
                    else if (fd == respIt->second.getCgiStdinFd())
                    {
                        if (ep.events[i].events & (EPOLLHUP | EPOLLERR))
                            respIt->second.closeCgiStdin(_cgiFdToClient);
                        else
                            respIt->second.flushCgiStdin(_cgiFdToClient);
                    }
                    continue;
                }

                if (ep.events[i].events & EPOLLHUP || ep.events[i].events & EPOLLRDHUP || ep.events[i].events & EPOLLERR)
                {
                    closeConnection(_req, _resp, fd);
                    continue;
                }
                if (ep.events[i].events & EPOLLIN)
                {
                    _req[fd]._start = time(NULL);

                    _req[fd].readRequest();
                    if (_req[fd].getIsRequestFinished())
                    {
                        _resp.insert(make_pair(fd, Response()));
                        ep.event.data.fd = fd;
                        ep.event.events = EPOLLIN | EPOLLOUT | EPOLLHUP | EPOLLRDHUP | EPOLLERR;
                        epoll_ctl(ep.epollFd, EPOLL_CTL_MOD, fd, &ep.event);
                    }
                }
                if (ep.events[i].events & EPOLLOUT && _req[fd].getIsRequestFinished())
                {
                    _resp[fd].sendResponse(_req[fd], fd, _cgiFdToClient);
                    if (_resp[fd].getIsFinished() == true)
                    {
                        if (_resp[fd].getKeepAlive())
                        {
                            logAccess(_req[fd], &_resp[fd]);
                            Server *srv = _req[fd].getServer();
                            string clientIp = _req[fd]._clientIp;
                            _req[fd] = Request(srv, fd, _servers);
                            _req[fd]._clientIp = clientIp;
                            _req[fd]._start = time(NULL);
                            gettimeofday(&_req[fd]._startTv, NULL);
                            _resp.erase(fd);
                            ep.event.data.fd = fd;
                            ep.event.events = EPOLLIN | EPOLLHUP | EPOLLRDHUP | EPOLLERR;
                            epoll_ctl(ep.epollFd, EPOLL_CTL_MOD, fd, &ep.event);
                        }
                        else
                            closeConnection(_req, _resp, fd);
                    }
                }
            }
            catch (const exception &e)
            {
                spdlog::error("Unhandled exception servicing fd {}: {}", fd, e.what());
                if (_req.find(fd) != _req.end())
                    safeCloseConnection(fd);
            }
            catch (...)
            {
                spdlog::error("Unknown exception servicing fd {}", fd);
                if (_req.find(fd) != _req.end())
                    safeCloseConnection(fd);
            }
        }
        for (map<int, Request>::iterator it = _req.begin(); it != _req.end(); ++it)
        {
            try
            {
                if (!it->second.getIsRequestFinished()
                    && (CLOCKWORK(it->second._start) > TIMEOUT
                        || CLOCKWORK(it->second._startTv.tv_sec) > REQUEST_TIMEOUT))
                {
                    it->second.setTimeout();
                    _resp.insert(make_pair(it->first, Response()));
                    ep.event.data.fd = it->first;
                    ep.event.events = EPOLLIN | EPOLLOUT | EPOLLHUP | EPOLLRDHUP | EPOLLERR;
                    epoll_ctl(ep.epollFd, EPOLL_CTL_MOD, it->first, &ep.event);
                }
            }
            catch (const exception &e)
            {
                spdlog::error("Unhandled exception in timeout scan for fd {}: {}", it->first, e.what());
            }
        }
    }
    for (map<int, Request>::iterator it = _req.begin(); it != _req.end(); ++it)
    {
        map<int, Response>::iterator respIt = _resp.find(it->first);
        if (respIt != _resp.end() && respIt->second._isCGI == true)
        {
            kill(respIt->second.pid, SIGKILL);
            waitpid(respIt->second.pid, 0, 0);
            respIt->second.closeCgiPipes(_cgiFdToClient);
        }
        logAccess(it->second, respIt != _resp.end() ? &respIt->second : NULL);
        epoll_ctl(ep.epollFd, EPOLL_CTL_DEL, it->first, NULL);
        close(it->first);
    }
    close(ep.epollFd);
    spdlog::info("Shutdown complete.");
}