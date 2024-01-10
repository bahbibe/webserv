#pragma once

#include "webserv.hpp"
#include "Server.hpp"
#include "Helpers.hpp"

#define BD_START "bd_start" // the start boundary and its headers
#define BD_CONTENT "bd_content" // the boundary content
#define BD_MID "bd_mid" // the mid boundary and it's headers 
#define BD_END  "bd_end" // the end boundary

class Boundaries {
    private:
        string _buffer;
        string _boundary;
        string _endBoundary;
        bool _isFileCreated;
        fstream *_outfile;

        string _state;
        string _bd_helper;

        string _bd_start;
        string _bd_body;
        string _bd_end;

        string _buffer_end;

    public:
        Boundaries();
        ~Boundaries();

        void parseBoundary(const string& buffer, const string& boundary);
        void checkStartBoundary();
        void checkEndBoundary();
        void writeBodyContent();
};