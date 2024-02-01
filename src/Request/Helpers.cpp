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

string& Helpers::generateFileName()
{
    static string fileName;
    fileName = "upload_";
    srand(time(NULL));
    for (int i = 0; i < 10; i++)
        fileName += (char)(rand() % 26 + 97);
    return fileName;
}


bool Helpers::isCGI(const string& path, vector<string>& indexs)
{
    if (path.find(".php") != string::npos || path.find(".py") != string::npos)
        return true;
    fstream file;
    for (size_t i = 0; i < indexs.size(); i++)
    {
        string filePath = path + indexs[i];
        file.open(filePath.c_str(), ios::in | ios::binary);
        if (file.is_open())
        {
            file.close();
            if (filePath.find(".php") != string::npos || filePath.find(".py") != string::npos)
                return true;
        }
    }
    return false;
}