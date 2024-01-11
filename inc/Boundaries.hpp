#pragma once

#include "webserv.hpp"
#include "Server.hpp"
#include "Helpers.hpp"

#define BD_START "bd_start"
#define BD_CONTENT "bd_content"
#define BD_MID "bd_mid"
#define BD_END  "bd_end"

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
        void parseBoundaryStart();
        void checkStartBoundary();
        void checkMidBoundary();
        void writeBodyContent();
};