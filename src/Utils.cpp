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
        stringstream ss(*it);
        int num;
        ss >> num;
        if (num < 0 || num > 255)
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

