#pragma once

#include "webserv.hpp"
#include "Server.hpp"
#include "Helpers.hpp"

#define BS_START "start"
#define BS_HEADERS "headers"
#define BS_BODY "body"
#define BS_END "end"

class Boundaries {
    private:
        string _buffer;
        size_t _index;
        string _boundary;
        string _state;
        bool _isFileCreated;
        fstream *_outfile;

        string _helperLine;

    public:
        Boundaries();
        ~Boundaries();

        void parseBoundary(const string& buffer, const string& boundary);

        void checkStartBoundary();
        void checkBoundaryHeaders();
        void checkBoundaryBody();
        void checkBoundaryEnds();
};