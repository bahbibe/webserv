#include "../inc/Server.hpp"

Server::Server()
{
    _autoindex = false;
}

Server::~Server()
{
    map<string, Location *>::iterator it = _locations.begin();
    for (; it != _locations.end(); it++)
    {
        delete it->second;
    }
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
    string tmp;
    Location *location = new Location();
    while (getline(ss, buff))
    {
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
                    throw runtime_error(ERR "Invalid autoindex");
            }
            else if (tmp == "cgi")
            {
                location->_dir.cgi++;
                line >> tmp;
                if (tmp == "on")
                    location->setCgi(true);
                else if (tmp != "off")
                    throw runtime_error(ERR "Invalid cgi");
            }
            else if (tmp == "upload")
            {
                location->_dir.upload++;
                line >> tmp;
                if (tmp == "on")
                    location->setUpload(true);
                else if (tmp != "off")
                    throw runtime_error(ERR "Invalid upload");
            }
            else if (tmp == "upload_path")
            {
                location->_dir.upload_path++;
                line >> tmp;
                location->setUploadPath(tmp);
            }
            else if (tmp == "return")
            {
                location->_dir.return_code++;
                line >> tmp;
                location->setReturn(tmp);
            }
        }
        else
            throw runtime_error(ERR "Invalid directive");
    }
    if (duplicateDirective(location->_dir))
    {
        printDirective(location->_dir);
        throw runtime_error(ERR "Duplicate directive");
    }
    return location;
}

void Server::parseServer(string const &file)
{
    stringstream ss(file);
    string buff;
    ss >> buff >> buff;
    memset(&_dir, 0, sizeof(t_dir));
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
                _locations[buff]->print();
                // exit(0);
            }
        }
        else
            throw runtime_error(ERR "Invalid directive" + buff);
    }
    if (duplicateDirective(_dir))
    {
        printDirective(_dir);
        throw runtime_error(ERR "Duplicate directive");
    }
}

void Server::print()
{
    // cout << "host: " + _host << endl;
    // cout << "port: " + _port << endl;
    // cout << "server_names: ";
    // for (vector<string>::iterator it = _server_names.begin(); it != _server_names.end(); it++)
    //     cout << *it << " ";
    // cout << "indexs: ";
    // for (vector<string>::iterator it = _indexs.begin(); it != _indexs.end(); it++)
    //     cout << *it << " ";
    // cout << "server_root: " + _server_root << endl;
    // cout << "error_pages: " << endl;
    // for (map<string, string>::iterator it = _error_pages.begin(); it != _error_pages.end(); it++)
    //     cout << it->first << " " << it->second << " " << endl;
    // cout << "client_max_body_size: " << _client_max_body_size << endl;
    // cout << "autoindex: " << _autoindex << endl;
    // cout << "locations: " << endl;
    // for (map<string, Location *>::iterator it = _locations.begin(); it != _locations.end(); it++)
    // {
    //     cout << "    " << it->first << endl;
    //     it->second->print();
    // }
}
void printDirective(t_dir dir)
{
    cout << "host: " << dir.host << endl;
    cout << "listen: " << dir.listen << endl;
    cout << "server_name: " << dir.server_name << endl;
    cout << "index: " << dir.index << endl;
    cout << "loc_index: " << dir.loc_index << endl;
    cout << "root: " << dir.root << endl;
    cout << "loc_root: " << dir.loc_root << endl;
    cout << "autoindex: " << dir.autoindex << endl;
    cout << "loc_autoindex: " << dir.loc_autoindex << endl;
    cout << "client_max_body_size: " << dir.client_max_body_size << endl;
    cout << "cgi: " << dir.cgi << endl;
    cout << "upload: " << dir.upload << endl;
    cout << "upload_path: " << dir.upload_path << endl;
    cout << "allow: " << dir.allow << endl;
    cout << "return_code: " << dir.return_code << endl;
}