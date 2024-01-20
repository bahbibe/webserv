#include "../../inc/webserv.hpp"
#include "../../inc/Server.hpp"

bool isServerDir(string const &dir)
{
    vector<string> directives;
    directives.push_back("host");
    directives.push_back("listen");
    directives.push_back("server_name");
    directives.push_back("root");
    directives.push_back("error_page");
    directives.push_back("client_max_body_size");
    directives.push_back("index");
    directives.push_back("autoindex");
    directives.push_back("location");
    vector<string>::iterator it = find(directives.begin(), directives.end(), dir);
    if (it != directives.end())
        return true;
    return false;
}

bool isLocationDir(string const &dir)
{
    vector<string> directives;
    directives.push_back("root");
    directives.push_back("return");
    directives.push_back("allow");
    directives.push_back("index");
    directives.push_back("autoindex");
    directives.push_back("cgi");
    directives.push_back("upload");
    directives.push_back("upload_path");
    directives.push_back("cgi_upload_path");
    vector<string>::iterator it = find(directives.begin(), directives.end(), dir);
    if (it != directives.end())
        return true;
    return false;
}

bool duplicateDirective(t_dir dir)
{
    for (size_t i = 0; i < sizeof(t_dir) / sizeof(int); i++)
    {
        if (((int *)&dir)[i] > 1)
            return true;
    }
    return false;
}

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
void Server::parseServer(string const &file)
{
    mimeTypes();
    stringstream ss(file);
    ss.seekg(Server::_pos);
    string buff;
    ss >> buff ;
    while (getline(ss, buff))
    {
        if (isBrackets(buff) && buff.find("}") != string::npos)
            break;
        if (buff.empty() || isWhitespace(buff) || isComment(buff) || isBrackets(buff))
            continue;
        stringstream line(buff);
        line >> buff;
        if (isServerDir(buff))
        {
            if (buff == "host")
            {
                _dir.host++;
                line >> _host;
                if (_host == "localhost")
                    _host = "127.0.0.1";
                else if (!isIpV4(_host))
                    throw ServerException(ERR "Invalid host");
            }
            else if (buff == "listen")
            {
                _dir.listen++;
                line >> _port;
                if (_port.empty())
                    _port = DEFAULT_PORT;
                if (!isNumber(_port))
                    throw ServerException(ERR "Invalid port");
            }
            else if (buff == "server_name")
            {
                _dir.server_name++;
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
                _dir.index++;
                while (line >> buff)
                    _indexs.push_back(buff);
            }
            else if (buff == "root")
            {
                _dir.root++;
                line >> _server_root;
            }
            else if (buff == "autoindex")
            {
                _dir.autoindex++;
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
                _dir.client_max_body_size++;
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
            throw ServerException(ERR "Invalid directive");
    }
    if (duplicateDirective(_dir))
        throw ServerException(ERR "Duplicate directive");
    Server::_pos = ss.tellg();
}
