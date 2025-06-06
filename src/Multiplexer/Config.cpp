#include "../../inc/webserv.hpp"
#include "../../inc/Server.hpp"

Location *Server::parseLocation(stringstream &ss)
{
    string buff;
    string tmp;
    Location *location = new Location();
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
                    location->setRoot(_server_root);
                location->setRoot(tmp);
            }
            else if (tmp == "autoindex")
            {
                location->_dir.autoindex++;
                line >> tmp;
                if (tmp == "on")
                    location->setAutoindex(true);
                else if (tmp != "off")
                    throw ServerException(ERR "Invalid autoindex");
            }
            else if (tmp == "cgi")
            {
                location->_dir.cgi++;
                line >> tmp;
                if (tmp == "on")
                    location->setCgi(true);
                else if (tmp != "off")
                    throw ServerException(ERR "Invalid cgi");
            }
            else if (tmp == "upload")
            {
                location->_dir.upload++;
                line >> tmp;
                if (tmp == "on")
                    location->setUpload(true);
                else if (tmp != "off")
                    throw ServerException(ERR "Invalid upload");
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
        }
        else
            throw ServerException(ERR "Invalid directive");
    }
    if (duplicateDirective(location->_dir))
        throw ServerException(ERR "Duplicate directive");
    if (location->getRoot().empty())
        location->setRoot(_server_root);
    if (location->_dir.autoindex == 0)
        location->setAutoindex(_autoindex);
    return location;
}
void Server::mimeTypes()
{
    ifstream mime;
    mime.open("conf/mime.types");
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
                _extensions[type].push_back(ext);
                _types[ext] = type;
            }
            ss.clear();
        }
        mime.close();
    }
    else
        throw Server::ServerException(ERR "Unable to open mime file");
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
        else if (!allowedConfig(buff))
            throw ServerException(ERR "Invalid config " + buff);
        if (isServerDir(buff))
        {
            if (buff == "host")
            {
                dir.host++;
                line >> _host;
                if (_host == "localhost")
                    _host = "127.0.0.1";
                else if (!isIpV4(_host))
                    throw ServerException(ERR "Invalid host");
            }
            else if (buff == "listen")
            {
                dir.listen++;
                line >> _port;
                if (_port.empty())
                    _port = DEFAULT_PORT;
                if (!isNumber(_port))
                    throw ServerException(ERR "Invalid port");
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
                    throw ServerException(ERR + _server_root + ": No such file or directory");
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
                    throw ServerException(ERR "Invalid autoindex");
            }
            else if (buff == "client_max_body_size")
            {
                dir.client_max_body_size++;
                line >> _client_max_body_size;
                if (!isNumber(_client_max_body_size))
                    throw ServerException(ERR "Invalid client_max_body_size");
            }
            else if (buff == "location")
            {
                line >> buff;
                _locations[buff] = parseLocation(ss);
            }
        }
        else
        {
            throw ServerException(ERR "Invalid directive");
        }
    }
    if (duplicateDirective(dir))
        throw ServerException(ERR "Duplicate directive");
    Server::_pos = ss.tellg();
    setupSocket();
}
