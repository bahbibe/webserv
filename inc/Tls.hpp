#pragma once
#include "webserv.hpp"
#include <memory>
#include <openssl/ssl.h>
#include <openssl/err.h>

// One TLS connection's handshake + read/write state, driven by the
// same non-blocking epoll loop as every plain-TCP connection in this
// project - no blocking SSL_accept()/SSL_read()/SSL_write() call
// anywhere. Owns the SSL* only; the underlying fd is owned (and
// closed) by Webserver as it already is for plain connections.
class TlsSession
{
public:
    TlsSession(SSL_CTX *ctx, int fd);
    ~TlsSession();

    TlsSession(TlsSession const &) = delete;
    TlsSession &operator=(TlsSession const &) = delete;

    // 1 = handshake complete, 0 = still in progress (wait for the
    // next epoll event - either direction, SSL_accept() can want
    // either regardless of which event just fired), -1 = fatal,
    // caller must close the connection.
    int handshake();
    bool isEstablished() const { return _established; }

    // Same "<=0 means not ready yet / try again next tick, don't
    // inspect errno" convention every plain read()/write() call site
    // in this project already follows for its non-blocking sockets.
    ssize_t read(char *buf, size_t len);
    ssize_t write(const char *buf, size_t len);

private:
    SSL *_ssl;
    bool _established;
};

// fd -> in-progress-or-established TLS session, populated by
// Webserver::newConnection() for connections accepted on an
// SSL-enabled listener, consulted by every plain read()/write() call
// site via tlsAwareRead()/tlsAwareWrite() below, and erased by
// Webserver's connection-teardown paths (erasing runs SSL_shutdown()
// + SSL_free() via ~TlsSession(), before the raw fd itself is closed).
extern map<int, unique_ptr<TlsSession> > g_tlsSessions;

ssize_t tlsAwareRead(int fd, char *buf, size_t len);
ssize_t tlsAwareWrite(int fd, const char *buf, size_t len);
