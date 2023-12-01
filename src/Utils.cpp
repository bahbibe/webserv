#include "../inc/webserv.hpp"

bool isWhitespace(const string &str)
{
    for (string::const_iterator it = str.begin(); it != str.end(); ++it)
    {
        if (!isspace(static_cast<unsigned char>(*it)))
            return false;
    }
    return true;
}

bool isComment(const string &str)
{
    string::const_iterator it = str.begin();
    while (it != str.end() && isspace(*it))
        ++it;
    return (it != str.end() && *it == '#');
}

bool isNumber(const string &str)
{
    for (string::const_iterator it = str.begin(); it != str.end(); ++it)
    {
        if (!isdigit(static_cast<unsigned char>(*it)))
            return false;
    }
    return true;
}

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

bool isServerDir(string const &dir)
{
    vector<string> directives;
    directives.push_back("server");
    directives.push_back("server_name");
    directives.push_back("listen");
    directives.push_back("root");
    directives.push_back("error_page");
    directives.push_back("client_max_body_size");
    directives.push_back("index");
    directives.push_back("autoindex");
    directives.push_back("location");
    directives.push_back("return");
    vector<string>::iterator it = find(directives.begin(), directives.end(), dir);
    if (it != directives.end())
        return true;
    return false;
}

bool isLocationDir(string const &dir)
{
    vector<string> directives;
    directives.push_back("root");
    directives.push_back("index");
    directives.push_back("autoindex");
    directives.push_back("client_max_body_size");
    directives.push_back("cgi");
    directives.push_back("upload");
    vector<string>::iterator it = find(directives.begin(), directives.end(), dir);
    if (it != directives.end())
        return true;
    return false;
}

bool isIpV4(string const &str)
{
    vector<string> octets;
    string::size_type pos = 0;
    string::size_type prev = 0;
    while ((pos = str.find('.', pos)) != string::npos)
    {
        octets.push_back(str.substr(prev, pos - prev));
        prev = ++pos;
    }
    octets.push_back(str.substr(prev, pos - prev));
    if (octets.size() != 4)
        return false;
    for (vector<string>::iterator it = octets.begin(); it != octets.end(); ++it)
    {
        if (!isNumber(*it))
            return false;
        if (stoi(*it) < 0 || stoi(*it) > 255)
            return false;
    }
    return true;
}