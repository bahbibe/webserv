#include "../inc/webserv.hpp"

Server::Server()
{
    _autoindex = false;
    _client_max_body_size = 0;
}

Server::~Server()
{
}

// void Server::parseServer(string const &file)
// {
//     stringstream ss(file);
//     string buff;
//     ss >> buff >> buff;
//     while (getline(ss, buff))
//     {
//         if (buff.empty() || isWhitespace(buff) || isComment(buff))
//             continue;
//         stringstream line(buff);
//         line >> buff;
//         if (isDirective(buff))
//         {
//             if(buff == "listen")
//             {
//                 line >> buff;
//                 if (buff.find(';') != string::npos)
//                     buff.erase(buff.find(';'));
//                 _port = buff;
//             }
//             else if(buff == "server_name")
//             {
//                 line >> buff;
//                 if (buff.find(';') != string::npos)
//                     buff.erase(buff.find(';'));
//                 _server_names.push_back(buff);
//             }
//             else if(buff == "error_page")
//             {
//                 line >> buff;
//                 if (buff.find(';') != string::npos)
//                     buff.erase(buff.find(';'));
//                 int code = atoi(buff.c_str());
//                 line >> buff;
//                 if (buff.find(';') != string::npos)
//                     buff.erase(buff.find(';'));
//                 _error_pages[code] = buff;
//             }
//             else if(buff == "index")
//             {
//                 line >> buff;
//                 if (buff.find(';') != string::npos)
//                     buff.erase(buff.find(';'));
//                 _indexs.push_back(buff);
//             }
//             else if(buff == "root")
//             {
//                 line >> buff;
//                 if (buff.find(';') != string::npos)
//                     buff.erase(buff.find(';'));
//                 _server_root = buff;
//             }
//             else if(buff == "autoindex")
//             {
//                 line >> buff;
//                 if (buff.find(';') != string::npos)
//                     buff.erase(buff.find(';'));
//                 if (buff == "on")
//                     _autoindex = true;
//                 else if (buff == "off")
//                     _autoindex = false;
//                 else
//                     throw runtime_error(ERR "Invalid autoindex value");
//             }
//             else if(buff == "client_max_body_size")
//             {
//                 line >> buff;
//                 if (buff.find(';') != string::npos)
//                     buff.erase(buff.find(';'));
//                 _client_max_body_size = atoi(buff.c_str());
//             }
//             else if(buff == "location")
//             {
//                 line >> buff;
//                 if (buff[0] != '/')
//                     throw runtime_error(ERR "Expected '/'");
//                 parseLocation(buff);
//             }
//             else
//                 throw runtime_error(ERR "Invalid directive");
//         }
//         else
//             throw runtime_error(ERR "Invalid directive");
//     }
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
        if (buff.find(SEMICOLON) == string::npos)
            throw runtime_error(ERR "Expected ';'");
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
                    _port = buff.substr(buff.find(COLON) + 1, buff.find(SEMICOLON) - buff.find(COLON) - 1);
                    buff.erase(buff.find(COLON));
                    _host = buff;
                }
                else
                    _port = buff;
            }
            else if(buff == "server_name")
            {
                _dir.server_name++;
                line >> buff;
                // cout << "server_name: " << buff << endl;
                // if (buff.find(';') != string::npos)
                //     buff.erase(buff.find(';'));
                // _server_names.push_back(buff);
            }
            // else if(buff == "error_page")
            // {
            //     _dir.error_page++;
            //     line >> buff;
            //     if (buff.find(';') != string::npos)
            //         buff.erase(buff.find(';'));
            //     int code = atoi(buff.c_str());
            //     line >> buff;
            //     if (buff.find(';') != string::npos)
            //         buff.erase(buff.find(';'));
            //     _error_pages[code] = buff;
            // }
            // else if(buff == "index")
            // {
            //     _dir.index++;
            //     line >> buff;
            //     if (buff.find(';') != string::npos)
            //         buff.erase(buff.find(';'));
            //     _indexs.push_back(buff);
            // }
            // else if(buff == "root")
            // {
            //     _dir.root++;
            //     line >> buff;
            //     if (buff.find(';') != string::npos)
            //         buff.erase(buff.find(';'));
            //     _server_root = buff;
            // }
            // else if(buff == "autoindex")
            // {
            //     _dir.autoindex++;
            //     line >> buff;
            //     if (buff.find(';') != string::npos)
            //         buff.erase(buff.find(';'));
            //     if (buff == "on")
            //         _autoindex = true;
            //     else if (buff == "off")
            //         _autoindex = false;
            //     else
            //         throw runtime_error(ERR "Invalid autoindex value");
            // }
            // else if(buff == "client_max_body_size")
            // {
            //     _dir.client_max_body_size++;
            //     line >> buff;
            //     if (buff.find(';') != string::npos)
            //         buff.erase(buff.find(';'));
            //     _client_max_body_size = atoi(buff.c_str());
            // }
            // else if(buff == "location")
            // {
            //     _dir.cgi++;
            //     line >> buff;
            //     if (buff[0] != '/')
            //         throw runtime_error(ERR "Expected '/'");
            //     parseLocation(buff);
            // }
            // else
            //     throw runtime_error(ERR "Invalid directive");
        }
            
    }
    print();
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

}
