#include "../../inc/Tls.hpp"

map<int, unique_ptr<TlsSession> > g_tlsSessions;

TlsSession::TlsSession(SSL_CTX *ctx, int fd) : _ssl(SSL_new(ctx)), _established(false)
{
    if (!_ssl)
        throw WebservException(ERR "SSL_new failed");
    SSL_set_fd(_ssl, fd);
    SSL_set_accept_state(_ssl);
}

TlsSession::~TlsSession()
{
    if (_ssl)
    {
        // Best-effort, single non-blocking attempt - a full bidirectional
        // shutdown would need its own retry-on-next-epoll-tick state
        // machine, and the connection is going away either way.
        SSL_shutdown(_ssl);
        SSL_free(_ssl);
    }
}

int TlsSession::handshake()
{
    if (_established)
        return 1;
    ERR_clear_error();
    int rc = SSL_accept(_ssl);
    if (rc == 1)
    {
        _established = true;
        return 1;
    }
    int err = SSL_get_error(_ssl, rc);
    if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
        return 0;
    return -1;
}

ssize_t TlsSession::read(char *buf, size_t len)
{
    ERR_clear_error();
    int n = SSL_read(_ssl, buf, (int)len);
    if (n > 0)
        return n;
    return 0;
}

ssize_t TlsSession::write(const char *buf, size_t len)
{
    ERR_clear_error();
    int n = SSL_write(_ssl, buf, (int)len);
    if (n > 0)
        return n;
    return 0;
}

ssize_t tlsAwareRead(int fd, char *buf, size_t len)
{
    map<int, unique_ptr<TlsSession> >::iterator it = g_tlsSessions.find(fd);
    if (it != g_tlsSessions.end())
        return it->second->read(buf, len);
    return ::read(fd, buf, len);
}

ssize_t tlsAwareWrite(int fd, const char *buf, size_t len)
{
    map<int, unique_ptr<TlsSession> >::iterator it = g_tlsSessions.find(fd);
    if (it != g_tlsSessions.end())
        return it->second->write(buf, len);
    return ::write(fd, buf, len);
}
