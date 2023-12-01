#include "Server.hpp"

Server::Server()
{
    _autoindex = false;
}

Server::~Server()
{
}

void Server::parseConfig(string const &file)
{
    brackets(file);
    parseServer(file);
}

void Server::setErrorCodes(string const &code, string const &buff)
{
    string codes[11] = {"400", "403", "404", "405", "411", "413", "414", "500", "501", "503", "505"};
    for (int i = 0; i < 11; i++)
        if (code == codes[i])
        {
            _error_pages[code] = buff;
            return;
        }
    throw runtime_error(ERR "Invalid error code");
}

Location *Server::parseLocation(stringstream &ss)
{
    string buff;
    Location *location = new Location();
    while (getline(ss, buff))
    {
        if (buff.empty() || isWhitespace(buff) || isComment(buff))
            continue;
        stringstream line(buff);
        line >> buff >> buff;
        if (isLocationDir(buff))
        {
            if (buff == "allow")
            {
                _dir.allow++;
                while (line >> buff)
                    location->setMethods(buff);
            }
            else if (buff == "index")
            {
                _dir.index++;
                while (line >> buff)
                    location->setIndexs(buff);
            }
            else if (buff == "root")
            {
                _dir.loc_root++;
                line >> buff;
                location->setRoot(buff);
            }
            else if (buff == "autoindex")
            {
                _dir.loc_autoindex++;
                line >> buff;
                if (buff == "on")
                    location->setAutoindex(true);
                else if (buff == "off")
                    location->setAutoindex(false);
                else
                    throw runtime_error(ERR "Invalid autoindex");
            }
            else if (buff == "cgi")
            {
                _dir.cgi++;
                line >> buff;
                if (buff == "on")
                    location->setCgi(true);
                else if (buff == "off")
                    location->setCgi(false);
                else
                    throw runtime_error(ERR "Invalid cgi");
            }
            else if (buff == "upload")
            {
                _dir.upload++;
                line >> buff;
                if (buff == "on")
                    location->setUpload(true);
                else if (buff == "off")
                    location->setUpload(false);
                else
                    throw runtime_error(ERR "Invalid upload");
            }
            else if (buff == "upload_path")
            {
                _dir.upload_path++;
                line >> buff;
                location->setUploadPath(buff);
            }
            else if (buff == "cgi_path")
            {
                string extention, path;
                line >> extention;
                if (extention != ".php" && extention != ".py")
                    throw runtime_error(ERR "Invalid cgi extention");
                line >> path;
                location->setCgiPath(extention, path);
            }
            else if (buff == "return")
            {
                _dir.return_code++;
                string code;
                line >> code;
                line >> buff;
                location->setReturn(code, buff);
            }
            else
                throw runtime_error(ERR "Invalid directive");
        }
    }
    return location;
}

bool duplicateDirective(t_dir dir)
{
    for (int i = 0; i < sizeof(t_dir) / sizeof(int); i++)
    {
        if (((int *)&dir)[i] > 1)
            return true;
    }
    return false;
}

void Server::parseServer(string const &file)
{
    stringstream ss(file);
    string buff;
    ss >> buff >> buff;
    while (getline(ss, buff))
    {
        if (buff.empty() || isWhitespace(buff) || isComment(buff))
            continue;
        stringstream line(buff);
        line >> buff;
        if (isServerDir(buff))
        {
            memset(&_dir, 0, sizeof(t_dir));
            if (buff == "host")
            {
                _dir.host++;
                line >> _host;
                if (_host == "localhost")
                    _host = "127.0.0.1";
                else if (!isIpV4(_host))
                    throw runtime_error(ERR "Invalid host");
            }
            else if (buff == "listen")
            {
                _dir.listen++;
                line >> _port;
                if (_port.empty())
                    _port = "80";
                if (!isNumber(_port))
                    throw runtime_error(ERR "Invalid port");
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
                    throw runtime_error(ERR "Invalid autoindex");
            }
            else if (buff == "client_max_body_size")
            {
                _dir.client_max_body_size++;
                line >> _client_max_body_size;
                if (!isNumber(_client_max_body_size))
                    throw runtime_error(ERR "Invalid client_max_body_size");
            }
            else if (buff == "location")
            {
                line >> buff;
                _locations[buff] = parseLocation(ss);
            }
            else
                throw runtime_error(ERR "Invalid directive");
        }
    }
    if (duplicateDirective(_dir))
        throw runtime_error(ERR "Duplicate directive");
}

void Server::print()
{
    cout << "host: " + _host << endl;
    cout << "port: " + _port << endl;
    cout << "server_names: ";
    for (vector<string>::iterator it = _server_names.begin(); it != _server_names.end(); it++)
        cout << *it << " ";
    cout << "indexs: ";
    for (vector<string>::iterator it = _indexs.begin(); it != _indexs.end(); it++)
        cout << *it << " "; 
    cout << "server_root: " + _server_root << endl;
    cout << "error_pages: " << endl;
    for (map<string, string>::iterator it = _error_pages.begin(); it != _error_pages.end(); it++)
        cout << it->first << " " << it->second << " " << endl;
    cout << "client_max_body_size: " << _client_max_body_size << endl;
    cout << "autoindex: " << _autoindex << endl;
    cout << "locations: " << endl;
    
}
