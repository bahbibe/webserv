#include "../inc/webserv.hpp"

bool isDirective(string const &line)
{
    vector<string> directives ;
    directives.push_back("host");
    directives.push_back("server_name");
    directives.push_back("listen");
    directives.push_back("error_page");
    directives.push_back("client_max_body_size");
    directives.push_back("root");
    directives.push_back("index");
    directives.push_back("autoindex");
    for (size_t i = 0; i < directives.size(); i++)
        if (line == directives[i])
            return true;
    return false;
}

void brackets(string const &buff)
{
    stringstream ss(buff);
    string line;
    int line_num = 0;
    stack<string> lim;
    while (ss >> line)
    {   
        line_num++;
        if (line == "server" || line == "location")
        {
            if (line == "location")
            {
                ss >> line;
                if (line[0] != '/')
                    throw runtime_error(ERR "Expected '/'");
            }
            ss >> line;
            if (line == OPEN_BR)
                lim.push(line);
            else
                throw runtime_error(ERR "Expected '{'");
        }
        else if (line == CLOSE_BR)
        {
            if (lim.top() == OPEN_BR)
                lim.pop();
            else
                throw runtime_error(ERR "Expected '}'");
        }
        else if (line_num == 1 && line != "server")
            throw runtime_error(ERR "Expected 'server'");
    }
    if (lim.size())
        throw runtime_error(ERR "Unclosed '{'");
}


void parseConfig(string const &buff)
{
    brackets(buff);
    stringstream ss(buff);
    string line;
    while (ss >> line)
    {
        if (line == "server")
        {
            ss >> line;
            while (ss >> line)
            {
                if (!isDirective(line))
                    throw runtime_error(ERR "Unknown directive");

            }
        }
        else
            throw runtime_error(ERR "Expected 'server'");
    }
}