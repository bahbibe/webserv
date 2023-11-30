#include "../inc/webserv.hpp"

void brackets(string const &file)
{
    stringstream ss(file);
    string buff;
    stack<string> lim;
    while (ss >> buff)
    {
        if (buff == "server")
        {
            ss >> buff;
            if (buff == OPEN_BR)
                lim.push(buff);
            else
                throw runtime_error(ERR "Expected '{'");
        }
        else if (buff == "location")
        {
            ss >> buff;
            if (buff[0] != '/')
                throw runtime_error(ERR "Expected '/'");
            ss >> buff;
            if (buff == OPEN_BR)
                lim.push(buff);
            else
                throw runtime_error(ERR "Expected '{'");
        }
        else if (buff == CLOSE_BR)
        {
            if (!lim.empty() && lim.top() == OPEN_BR)
                lim.pop();
            else
                throw runtime_error(ERR "Expected '}'");
        }
    }
    if (!lim.empty())
        throw runtime_error(ERR "Unclosed '{'");
}

void Server::parseConfig(string const &file)
{
    brackets(file);
    parseServer(file);
}
