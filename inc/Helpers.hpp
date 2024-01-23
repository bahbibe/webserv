#pragma once

#include "webserv.hpp"
using namespace std;

class Helpers {
    public:
        static bool checkURICharSet(const string& requestURI);
        static bool checkLineEnd(const string& line);
        static bool decodeURI(string& requestURI);
        static string& generateFileName();
        static string findExtension(const string& mimeType);
};