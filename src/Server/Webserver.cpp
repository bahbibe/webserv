#include "../../inc/webserv.hpp"
#include "../../inc/Server.hpp"
#include "../../inc/Request.hpp"
#include "../../inc/Response.hpp"
#include "../../inc/Tls.hpp"

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
    if (server.getSsl())
    {
        // SSL_accept() can want either direction on its very first
        // call regardless of which epoll event woke us - register
        // both until the handshake settles, then drop back to
        // EPOLLIN-only (see the handshake routing in start()).
        g_tlsSessions[clientSock] = make_unique<TlsSession>(server.getSslCtx(), clientSock);
        ep.event.events = EPOLLIN | EPOLLOUT | EPOLLHUP | EPOLLRDHUP | EPOLLERR;
    }
    else
        ep.event.events = EPOLLIN | EPOLLHUP | EPOLLRDHUP | EPOLLERR;
    if (epoll_ctl(ep.epollFd, EPOLL_CTL_ADD, clientSock, &ep.event))
    {
        g_tlsSessions.erase(clientSock);
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
    g_tlsSessions.erase(sock);
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
        g_tlsSessions.erase(sock);
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

                map<int, unique_ptr<TlsSession> >::iterator tlsIt = g_tlsSessions.find(fd);
                if (tlsIt != g_tlsSessions.end() && !tlsIt->second->isEstablished())
                {
                    int rc = tlsIt->second->handshake();
                    if (rc < 0)
                        closeConnection(_req, _resp, fd);
                    else if (rc == 1)
                    {
                        ep.event.data.fd = fd;
                        ep.event.events = EPOLLIN | EPOLLHUP | EPOLLRDHUP | EPOLLERR;
                        epoll_ctl(ep.epollFd, EPOLL_CTL_MOD, fd, &ep.event);
                    }
                    continue;
                }

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
                    {
                        // relayCgiOutput() only queues bytes into the
                        // Response's own buffer - it never writes to
                        // the client socket itself (see Cgi.cpp). Its
                        // EPOLLOUT was stripped while nothing was
                        // ready (see CGI()'s first-call branch); only
                        // re-arm it once there's actually something -
                        // a header, a body chunk, or the final EOF
                        // terminator - for sendResponse() to flush.
                        if (respIt->second.relayCgiOutput(_req[clientFd], _cgiFdToClient))
                        {
                            ep.event.data.fd = clientFd;
                            ep.event.events = EPOLLIN | EPOLLOUT | EPOLLHUP | EPOLLRDHUP | EPOLLERR;
                            epoll_ctl(ep.epollFd, EPOLL_CTL_MOD, clientFd, &ep.event);
                        }
                    }
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
                    // Once the response is fully sent, leave it alone even
                    // if EPOLLOUT keeps firing (an always-writable socket
                    // fires it every tick) - a POST can still be draining
                    // its declared body at this point, and re-entering
                    // sendResponse() after it already reported finished
                    // would re-run its "close out the stream" branches and
                    // send a duplicate terminator.
                    if (!_resp[fd].getIsFinished())
                        _resp[fd].sendResponse(_req[fd], fd, _cgiFdToClient);
                    if (_resp[fd].getIsFinished() == true && !_req[fd].isDraining())
                    {
                        if (_resp[fd].getKeepAlive() && !_req[fd].isDrainTimedOut())
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
                // A POST draining its leftover declared body after an
                // early error/success can't get another timeout via the
                // branch above (isRequestFinished is already true) - if
                // the client stalls or trickles it out past the same
                // idle/absolute limits, abort the drain so the response
                // dispatch above forces a close instead of reusing the
                // connection with unread bytes still on the wire.
                else if (it->second.isDraining()
                    && (CLOCKWORK(it->second._start) > TIMEOUT
                        || CLOCKWORK(it->second._startTv.tv_sec) > REQUEST_TIMEOUT))
                {
                    it->second.abortDraining();
                }
                // A CGI response with its client-socket EPOLLOUT
                // stripped (see CGI()'s first-call branch) only gets
                // driven again by the client socket once
                // relayCgiOutput() finds real output to relay. A
                // script producing no output at all - hung, or just
                // silent until it exits - would never trigger that,
                // so CGI()'s own child-reap and CGI_TIMEOUT check
                // (normally piggybacked on that per-tick call) would
                // never run either. This keeps both alive at the same
                // ~1s cadence epoll_wait's own timeout already gives
                // every other periodic check in this loop, instead of
                // depending on a busy-spinning socket to provide it.
                map<int, Response>::iterator cgiRespIt = _resp.find(it->first);
                if (cgiRespIt != _resp.end() && cgiRespIt->second._isCGI && !cgiRespIt->second.getIsFinished())
                {
                    cgiRespIt->second.sendResponse(it->second, it->first, _cgiFdToClient);
                    // Rare: the call above hit real backpressure (e.g.
                    // the CGI_TIMEOUT finalizer's small write didn't
                    // fully land). Re-arm EPOLLOUT so the socket's own
                    // writability resumes it promptly instead of
                    // waiting up to another ~1s for this same scan.
                    if (cgiRespIt->second.hasPendingOutput())
                    {
                        ep.event.data.fd = it->first;
                        ep.event.events = EPOLLIN | EPOLLOUT | EPOLLHUP | EPOLLRDHUP | EPOLLERR;
                        epoll_ctl(ep.epollFd, EPOLL_CTL_MOD, it->first, &ep.event);
                    }
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
        g_tlsSessions.erase(it->first);
        close(it->first);
    }
    close(ep.epollFd);
    spdlog::info("Shutdown complete.");
}