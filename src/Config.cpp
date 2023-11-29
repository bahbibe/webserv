#include "../inc/webserv.hpp"

void brackets(string const &file)
{
    stringstream ss(file);
    string buff;
    int line_num = 0;
    stack<string> lim;
    while (ss >> buff)
    {
        line_num++;
        if (buff == "server" || buff == "location")
        {
            if (buff == "location")
            {
                ss >> buff;
                if (buff[0] != '/')
                    throw runtime_error(ERR "Expected '/'");
            }
            ss >> buff;
            if (buff == OPEN_BR)
                lim.push(buff);
            else
                throw runtime_error(ERR "Expected '{'");
        }
        else if (buff == CLOSE_BR)
        {
            if (lim.top() == OPEN_BR)
                lim.pop();
            else
                throw runtime_error(ERR "Expected '}'");
        }
        else if (line_num == 1 && buff != "server")
            throw runtime_error(ERR "Expected 'server'");
    }
    if (lim.size())
        throw runtime_error(ERR "Unclosed '{'");
}


void parseConfig(string const &file)
{
    brackets(file);
    Server *server = new Server();
    server->parseServer(file);
    delete server;
}
