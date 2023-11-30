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

bool isDirective(string const &dir)
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