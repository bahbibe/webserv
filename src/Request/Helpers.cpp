#include "../../inc/Helpers.hpp"

bool Helpers::checkURICharSet(const string& requestURI)
{
    std::string validCharSet("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~:/?#[]@!$&'()*+,;=%");
    for (size_t i = 0; i < requestURI.length(); i++)
    {
        if (validCharSet.find(requestURI[i]) == std::string::npos)
            return false;
    }
    return true;
}

bool Helpers::checkLineEnd(const string& line)
{
    size_t size = line.length();
    return (size > 2 && line[size - 2] == '\r' && line[size - 1] == '\n');
}


bool Helpers::decodeURI(string& requestURI)
{
    string decodedURI;
    string hex;
    for (size_t i = 0; i < requestURI.length(); i++)
    {
        if (requestURI[i] == '%')
        {
            if (i + 2 >= requestURI.length())
                return false;
            hex = requestURI.substr(i + 1, 2);
            for (size_t i = 0; i < hex.length(); i++)
                if (!isxdigit(hex[i]))
                    return false;
            char c = (char)strtol(hex.c_str(), NULL, 16);
            decodedURI += c;
            i += 2;
        } else
            decodedURI += requestURI[i];
    }
    requestURI = decodedURI;
    return true;
}