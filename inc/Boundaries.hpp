#pragma once

#include "webserv.hpp"
#include "Server.hpp"
#include "Helpers.hpp"
#include <sys/time.h>

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
        string _uploadPath;

        string _state;
        string _bd_start;
        string _rest;
        size_t _filesCounter;
        map<string, vector<string> > _mimeTypes;

    public:
        Boundaries();
        // Boundaries(const Boundaries& other);
        // Boundaries& operator=(const Boundaries& rhs);
        ~Boundaries();

        void parseBoundary(const string& buffer, const string& boundary, int readBytes, const string& uploadPath);
        void checkFirstBoundary();
        void handleBoundaries();
        void writeContent();
        string getExtension();
        void createFile();
        void throwException(int code);
        void closeOutFile();
        void setMimeTypes(map<string, vector<string> > mimeTypes);
};