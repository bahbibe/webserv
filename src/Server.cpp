#include "../inc/webserv.hpp"

Server::Server()
{
    _autoindex = false;
    _client_max_body_size = 0;
}

Location::Location()
{
    _autoindex = false;
    _upload = false;
    _cgi = false;
}

Location::~Location()
{

}
Server::~Server()
{

}


Location *Server::parseLocation(stringstream &ss)
{
    cout << 
    Location *location = new Location();
    // stringstream ss(buff);
    // string tmp;
    // ss >> tmp;
    // if (tmp != "location")
    //     throw runtime_error(ERR "Expected 'location'");
    // ss >> tmp;
    // if (tmp[0] != '/')
    //     throw runtime_error(ERR "Expected '/'");
    // location->return_code = tmp;
    // ss >> tmp;
    // if (tmp != "{")
    //     throw runtime_error(ERR "Expected '{'");
    // while (getline(ss, tmp))
    // {
    //     if (tmp.empty() || isWhitespace(tmp) || isComment(tmp))
    //         continue;
    //     stringstream line(tmp);
    //     line >> tmp;
    //     if (isDirective(tmp)) //!TODO: add check for duplicate directives
    //     {
    //         memset(&_dir, 0, sizeof(t_dir));
    //         if(tmp == "methods")
    //         {
    //             _dir.methods++;
    //             while (line >> tmp)
    //                 location->_methods.push_back(tmp);
    //         }
    //         else if(tmp == "index")
    //         {
    //             _dir.index++;
    //             while (line >> tmp)
    //                 location->_indexs.push_back(tmp);
    //         }
    //         else if(tmp == "root")
    //         {
    //             _dir.root++;
    //             line >> location->_root;
    //         }
    //         else if(tmp == "upload_path")
    //         {
    //             _dir.upload++;
    //             line >> location->_upload_path;
    //         }
    //         else if(tmp == "autoindex")
    //         {
    //             _dir.autoindex++;
    //             line >> tmp;
    //             if (tmp == "on")
    //                 location->_autoindex = true;
    //         }
    //         else if(tmp == "cgi")
    //         {
    //             _dir.cgi++;
    //             string key;
    //             string value;
    //             line >> key;
    //             line >> value;
    //             location->_cgi_path[key] = value;
    //         }
    //         else
    //             throw runtime_error(ERR "Invalid directive");
    //     }
    //     else
    //         throw runtime_error(ERR "Invalid directive" + tmp);
    // }
    return location;
}
// {
//     Location *location = new Location();
//     ss.seekg(pos);
//     string buff;
//     while (ss >> buff)
//     {
//         if (buff == "{")
//             break;
//         else if (buff == "=")
//         {
//             ss >> buff;
//             if (buff[0] != '/')
//                 throw runtime_error(ERR "Expected '/'");
//             break;
//         }
//         else
//             throw runtime_error(ERR "Expected '{' or '='");
//     }
    // string buff;
    // line >> buff;
    // if (buff[0] != '/')
    //     throw runtime_error(ERR "Expected '/'");
    // while (line >> buff)
    // {
    //     if (buff == "{")
    //         break;
    //     else if (buff == "=")
    //     {
    //         line >> buff;
    //         if (buff[0] != '/')
    //             throw runtime_error(ERR "Expected '/'");
    //         break;
    //     }
    //     else
    //         throw runtime_error(ERR "Expected '{' or '='");
    // }
    // line.seekg(pos);
    // cout << line.str() << endl;
// }


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
        if (isDirective(buff)) //!TODO: add check for duplicate directives
        {
            memset(&_dir, 0, sizeof(t_dir));
            if(buff == "listen")
            {
                _dir.listen++;
                line >> buff;
                if (buff.find(COLON) != string::npos)
                {
                    _port = buff.substr(buff.find(COLON) + 1);
                    buff.erase(buff.find(COLON));
                    _host = buff;
                }
                else
                    _port = buff;
            }
            else if(buff == "server_name")
            {
                _dir.server_name++;
                while (line >> buff)
                    _server_names.push_back(buff);
            }
            else if(buff == "error_page")
            {
                int code;
                line >> code;
                line >> buff;
                _error_pages[code] = buff;
            }
            else if(buff == "index")
            {
                _dir.index++;
                while (line >> buff)
                    _indexs.push_back(buff);
            }
            else if(buff == "root")
            {
                _dir.root++;
                line >> _server_root;
            }
            else if(buff == "autoindex")
            {
                _dir.autoindex++;
                line >> buff;
                if (buff == "on")
                    _autoindex = true;
            }
            else if(buff == "client_max_body_size")
            {
                _dir.client_max_body_size++;
                line >> _client_max_body_size;
            }
            else if(buff == "location")
            {
                _locations.push_back(parseLocation(ss));
                // streampos pos = ss.tellg();
                // this->
            }
            else
                throw runtime_error(ERR "Invalid directive");
        }
        else
            throw runtime_error(ERR "Invalid directive" + buff);
    }
}

void Server::print()
{
    cout << "host: " + _host << endl;
    cout << "port: " + _port << endl;
    cout << "server_root: " + _server_root << endl;
    cout << "autoindex: " << _autoindex << endl;
    cout << "client_max_body_size: " << _client_max_body_size << endl;
    cout << "server_names: ";
    for (vector<string>::iterator it = _server_names.begin(); it != _server_names.end(); it++)
        cout << *it << " ";
    cout << endl;
    cout << "error_pages: " << endl;
    for (map<int, string>::iterator it = _error_pages.begin(); it != _error_pages.end(); it++)
        cout << it->first << " " << it->second << " " << endl;
    cout << "indexs: ";
    for (vector<string>::iterator it = _indexs.begin(); it != _indexs.end(); it++)
        cout << *it << " ";
    
}
