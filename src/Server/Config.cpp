#include "../../inc/webserv.hpp"
#include "../../inc/Server.hpp"

unique_ptr<Location> Server::parseLocation(stringstream &ss)
{
    string buff;
    string tmp;
    unique_ptr<Location> location = make_unique<Location>();
    while (getline(ss, buff))
    {
        trim(buff);
        if (isBrackets(buff) && buff.find("}") != string::npos)
            break;
        if (buff.empty() || isWhitespace(buff) || isComment(buff) || isBrackets(buff))
            continue;
        stringstream line(buff);
        line >> tmp;
        if (isLocationDir(tmp))
        {
            if (tmp == "allow")
            {
                location->_dir.allow++;
                while (line >> tmp)
                    location->setMethods(tmp);
            }
            else if (tmp == "index")
            {
                location->_dir.index++;
                while (line >> tmp)
                    location->setIndexs(tmp);
            }
            else if (tmp == "root")
            {
                location->_dir.root++;
                line >> tmp;
                if (access(tmp.c_str(), F_OK) == -1)
                {
                    configErrors.add(ERR + tmp + ": No such file or directory");
                    location->setRoot(_server_root);
                }
                else
                    location->setRoot(tmp);
            }
            else if (tmp == "autoindex")
            {
                location->_dir.autoindex++;
                line >> tmp;
                if (tmp == "on")
                    location->setAutoindex(true);
                else if (tmp != "off")
                    configErrors.add(ERR "Invalid autoindex value: " + tmp);
            }
            else if (tmp == "cgi")
            {
                location->_dir.cgi++;
                line >> tmp;
                if (tmp == "on")
                    location->setCgi(true);
                else if (tmp != "off")
                    configErrors.add(ERR "Invalid cgi value: " + tmp);
            }
            else if (tmp == "upload")
            {
                location->_dir.upload++;
                line >> tmp;
                if (tmp == "on")
                    location->setUpload(true);
                else if (tmp != "off")
                    configErrors.add(ERR "Invalid upload value: " + tmp);
            }
            else if (tmp == "upload_path")
            {
                location->_dir.upload_path++;
                line >> tmp;
                location->setUploadPath(tmp);
            }
            else if (tmp == "cgi_upload_path")
            {
                location->_dir.cgi_upload_path++;
                line >> tmp;
                location->setCgiUploadPath(tmp);
            }
            else if (tmp == "return")
            {
                location->_dir.return_code++;
                line >> tmp;
                location->setReturn(tmp);
            }
            else if (tmp == "cgi_path")
            {
                string ext, interpreter;
                line >> ext >> interpreter;
                if (ext.empty() || interpreter.empty())
                    configErrors.add(ERR "Invalid cgi_path directive (needs an extension and an interpreter)");
                else
                    location->setCgiPath(ext, interpreter);
            }
        }
        else
            configErrors.add(ERR "Invalid directive in location block: " + tmp);
    }
    if (duplicateDirective(location->_dir))
        configErrors.add(ERR "Duplicate directive in a location block");
    if (location->getRoot().empty())
        location->setRoot(_server_root);
    if (location->_dir.autoindex == 0)
        location->setAutoindex(_autoindex);
    return location;
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
string toStr(int i)
{
    stringstream ss;
    ss << i;
    return ss.str();
}
void Server::parseServer(string const &file)
{
    mimeTypes();
    stringstream ss(file);
    ss.seekg(_pos);
    string buff;
    t_dir dir;
    memset(&dir, 0, sizeof(t_dir));
    while (getline(ss, buff))
    {
        trim(buff);
        if (isBrackets(buff) && buff.find("}") != string::npos)
            break;
        if (buff.empty() || isWhitespace(buff) || isComment(buff) )
            continue;
        stringstream line(buff);
        line >> buff;
        if (buff == "server" || buff == "}")
            continue;
        if (isServerDir(buff))
        {
            if (buff == "host")
            {
                dir.host++;
                line >> _host;
                if (_host == "localhost")
                    _host = "127.0.0.1";
                else if (resolveHostFamily(_host) == -1)
                    configErrors.add(ERR "Invalid host: " + _host);
            }
            else if (buff == "listen")
            {
                dir.listen++;
                line >> _port;
                if (_port.empty())
                    _port = DEFAULT_PORT;
                if (!isNumber(_port))
                    configErrors.add(ERR "Invalid port: " + _port);
            }
            else if (buff == "server_name")
            {
                dir.server_name++;
                while (line >> buff)
                    _server_names.push_back(buff);
            }
            else if (buff == "error_page")
            {
                string code;
                line >> code;
                line >> buff;
                setErrorCodes(code, buff);
            }
            else if (buff == "index")
            {
                dir.index++;
                while (line >> buff)
                    _indexs.push_back(buff);
            }
            else if (buff == "root")
            {
                dir.root++;
                line >> _server_root;
                if (access(_server_root.c_str(), F_OK) == -1)
                    configErrors.add(ERR + _server_root + ": No such file or directory");
            }
            else if (buff == "autoindex")
            {
                dir.autoindex++;
                line >> buff;
                if (buff == "on")
                    _autoindex = true;
                else if (buff == "off")
                    _autoindex = false;
                else
                    configErrors.add(ERR "Invalid autoindex value: " + buff);
            }
            else if (buff == "client_max_body_size")
            {
                dir.client_max_body_size++;
                line >> _client_max_body_size;
                if (!isNumber(_client_max_body_size))
                    configErrors.add(ERR "Invalid client_max_body_size: " + _client_max_body_size);
            }
            else if (buff == "location")
            {
                line >> buff;
                _locations[buff] = parseLocation(ss);
            }
        }
        else
        {
            configErrors.add(ERR "Invalid directive at server level: " + buff);
        }
    }
    if (duplicateDirective(dir))
        configErrors.add(ERR "Duplicate directive in server " + (_host.empty() ? string("(unknown host)") : _host));
    Server::_pos = ss.tellg();
}
