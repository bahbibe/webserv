#include "../../inc/webserv.hpp"

bool isWhitespace(const string &str)
{
    for (string::const_iterator it = str.begin(); it != str.end(); ++it)
    {
        if (!isspace(static_cast<unsigned char>(*it)))
            return false;
    }
    return true;
}

void trim(string &str)
{
    size_t start = str.find_first_not_of(" \t");
    size_t end = str.find_last_not_of(" \t");
    if (start == string::npos || end == string::npos)
        str = "";
    else
        str = str.substr(start, end - start + 1);
}

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
    directives.push_back("cgi_path");
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
int resolveHostFamily(string const &host)
{
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_NUMERICHOST;
    if (getaddrinfo(host.c_str(), NULL, &hints, &res) != 0)
        return -1;
    int family = res->ai_family;
    freeaddrinfo(res);
    return family;
}

bool isComment(const string &str)
{
    string::const_iterator it = str.begin();
    while (it != str.end() && isspace(*it))
        ++it;
    return (it != str.end() && *it == '#');
}

bool isBrackets(const string &str)
{
    string::const_iterator it = str.begin();
    while (it != str.end() && isspace(*it))
        ++it;
    return (it != str.end() && (*it == '{' || *it == '}'));
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

void addConfigError(string const &msg)
{
    configErrors.push_back(msg);
}

