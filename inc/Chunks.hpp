#pragma once

#include "webserv.hpp"
#include "Server.hpp"
#include "Helpers.hpp"

#define CH_START "ch_start"
#define CH_SIZE "ch_size"
#define CH_CONTENT "ch_content"

class Chunks {
    private:
        string _buffer;
        string _state;
        string _helper;

        fstream *_outfile;
        string _filePath;
        size_t _chunkSize;
        size_t _writedContent;
        int _nextBufferSize;
        size_t _clientMaxBodySize;
    
    public:
        Chunks();
        ~Chunks();

        void setChunks(fstream *outfile, const string& filePath, size_t clientMaxBodySize);
        int parse(const string& buffer, int readBytes);
        // int parse(const string& buffer, fstream *outfile, const string& filePath, int readBytes);
        void checkHexSize(const string& size);
        void setFirstSize();
        void setSize();
        void writeContent();
        void throwException(int code);
};
