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