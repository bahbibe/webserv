#include "../../inc/webserv.hpp"
#include "../../inc/Server.hpp"

// Directive application for one server block. ConfigParser owns
// tokenizing and block structure, recognizes the directive name via
// isServerDir(), and hands the name plus every value token sharing
// its source line here - "location" is handled entirely by
// ConfigParser itself, since it's structural, not a value-only
// directive, and never reaches this function. Same per-directive
// validation this project has always had, just reading from an
// already-tokenized vector instead of doing its own text scanning to
// get there.
void Server::applyServerDirective(string const &name, vector<string> const &values, t_dir &dir)
{
    size_t idx = 0;
    auto next = [&]() -> string { return idx < values.size() ? values[idx++] : string(); };

    if (name == "host")
    {
        dir.host++;
        _host = next();
        if (_host == "localhost")
            _host = "127.0.0.1";
        else if (resolveHostFamily(_host) == -1)
            configErrors.add(ERR "Invalid host: " + _host);
    }
    else if (name == "listen")
    {
        dir.listen++;
        _port = next();
        if (_port.empty())
            _port = DEFAULT_PORT;
        if (!isNumber(_port))
            configErrors.add(ERR "Invalid port: " + _port);
        if (idx < values.size())
        {
            string opt = next();
            if (opt == "ssl")
                _ssl = true;
            else
                configErrors.add(ERR "Invalid listen option: " + opt);
        }
    }
    else if (name == "ssl_certificate")
    {
        dir.ssl_certificate++;
        _sslCertPath = next();
        if (access(_sslCertPath.c_str(), F_OK) == -1)
            configErrors.add(ERR + _sslCertPath + ": No such file or directory");
    }
    else if (name == "ssl_certificate_key")
    {
        dir.ssl_certificate_key++;
        _sslKeyPath = next();
        if (access(_sslKeyPath.c_str(), F_OK) == -1)
            configErrors.add(ERR + _sslKeyPath + ": No such file or directory");
    }
    else if (name == "server_name")
    {
        dir.server_name++;
        while (idx < values.size())
            _server_names.push_back(next());
    }
    else if (name == "error_page")
    {
        string code = next();
        string path = next();
        setErrorCodes(code, path);
    }
    else if (name == "index")
    {
        dir.index++;
        while (idx < values.size())
            _indexs.push_back(next());
    }
    else if (name == "root")
    {
        dir.root++;
        _server_root = next();
        if (access(_server_root.c_str(), F_OK) == -1)
            configErrors.add(ERR + _server_root + ": No such file or directory");
    }
    else if (name == "autoindex")
    {
        dir.autoindex++;
        string v = next();
        if (v == "on")
            _autoindex = true;
        else if (v == "off")
            _autoindex = false;
        else
            configErrors.add(ERR "Invalid autoindex value: " + v);
    }
    else if (name == "client_max_body_size")
    {
        dir.client_max_body_size++;
        _client_max_body_size = next();
        if (!isNumber(_client_max_body_size))
            configErrors.add(ERR "Invalid client_max_body_size: " + _client_max_body_size);
    }
}

// Runs once, right after a server block's closing brace - the
// duplicate-directive check and the "listen ... ssl needs both cert
// files" check both need the whole block seen first.
void Server::finalizeServerDirectives(t_dir const &dir)
{
    if (duplicateDirective(dir))
        configErrors.add(ERR "Duplicate directive in server " + (_host.empty() ? string("(unknown host)") : _host));
    if (_ssl && (_sslCertPath.empty() || _sslKeyPath.empty()))
        configErrors.add(ERR "listen ... ssl needs both ssl_certificate and ssl_certificate_key");
}

void Server::addLocation(string const &path, unique_ptr<Location> location)
{
    _locations[path] = move(location);
}

// Directive application for one location block - mirrors
// applyServerDirective() above exactly. An invalid root path is
// recorded as an error but not immediately replaced with the
// server's own root here (unlike the pre-Phase-4 code, which did
// that inline): finalizeLocationDirectives() below already falls
// back to the server's root whenever this location's own root is
// still empty, which covers "never given" and "given but invalid"
// the same way, so there's no need for two separate fallback sites
// doing the same thing.
void Location::applyLocationDirective(string const &name, vector<string> const &values)
{
    size_t idx = 0;
    auto next = [&]() -> string { return idx < values.size() ? values[idx++] : string(); };

    if (name == "allow")
    {
        _dir.allow++;
        while (idx < values.size())
            setMethods(next());
    }
    else if (name == "index")
    {
        _dir.index++;
        while (idx < values.size())
            setIndexs(next());
    }
    else if (name == "root")
    {
        _dir.root++;
        string path = next();
        if (access(path.c_str(), F_OK) == -1)
            configErrors.add(ERR + path + ": No such file or directory");
        else
            setRoot(path);
    }
    else if (name == "autoindex")
    {
        _dir.autoindex++;
        string v = next();
        if (v == "on")
            setAutoindex(true);
        else if (v != "off")
            configErrors.add(ERR "Invalid autoindex value: " + v);
    }
    else if (name == "cgi")
    {
        _dir.cgi++;
        string v = next();
        if (v == "on")
            setCgi(true);
        else if (v != "off")
            configErrors.add(ERR "Invalid cgi value: " + v);
    }
    else if (name == "upload")
    {
        _dir.upload++;
        string v = next();
        if (v == "on")
            setUpload(true);
        else if (v != "off")
            configErrors.add(ERR "Invalid upload value: " + v);
    }
    else if (name == "upload_path")
    {
        _dir.upload_path++;
        setUploadPath(next());
    }
    else if (name == "cgi_upload_path")
    {
        _dir.cgi_upload_path++;
        setCgiUploadPath(next());
    }
    else if (name == "return")
    {
        _dir.return_code++;
        setReturn(next());
    }
    else if (name == "cgi_path")
    {
        string ext = next();
        string interpreter = next();
        if (ext.empty() || interpreter.empty())
            configErrors.add(ERR "Invalid cgi_path directive (needs an extension and an interpreter)");
        else
            setCgiPath(ext, interpreter);
    }
    else if (name == "client_max_body_size")
    {
        _dir.client_max_body_size++;
        string v = next();
        if (!isNumber(v))
            configErrors.add(ERR "Invalid client_max_body_size: " + v);
        else
        {
            size_t size = 0;
            stringstream(v) >> size;
            setClientMaxBodySize(size);
        }
    }
}

// Runs once, right after a location block's closing brace: the
// duplicate-directive check, and falling back to the owning server's
// root/autoindex/client_max_body_size for anything this location
// didn't set (validly) itself.
void Location::finalizeLocationDirectives(string const &serverRoot, bool serverAutoindex, size_t serverClientMaxBodySize)
{
    if (duplicateDirective(_dir))
        configErrors.add(ERR "Duplicate directive in a location block");
    if (getRoot().empty())
        setRoot(serverRoot);
    if (_dir.autoindex == 0)
        setAutoindex(serverAutoindex);
    if (_dir.client_max_body_size == 0)
        setClientMaxBodySize(serverClientMaxBodySize);
}

void Server::mimeTypes()
{
    static map<string, vector<string> > cachedExtensions;
    static map<string, string> cachedTypes;
    static bool loaded = false;
    if (!loaded)
    {
        ifstream mime;
        mime.open((confDir + "mime.types").c_str());
        if (mime.is_open())
        {
            string buff;
            stringstream ss;
            string type, ext;
            while (getline(mime, buff))
            {
                if (buff.empty() || isWhitespace(buff) || isComment(buff))
                    continue;
                ss << buff;
                ss >> type;
                while (ss >> ext)
                {
                    cachedExtensions[type].push_back(ext);
                    cachedTypes[ext] = type;
                }
                ss.clear();
            }
            mime.close();
            loaded = true;
        }
        else
            throw WebservException(ERR "Unable to open mime file");
    }
    _extensions = cachedExtensions;
    _types = cachedTypes;
}
