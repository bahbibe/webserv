#pragma once

#include "webserv.hpp"
#include "Server.hpp"
#include "Helpers.hpp"

#define CH_START "ch_start"
#define CH_SIZE "ch_size"
#define CH_CONTENT "ch_content"
#define CH_TRAILER "ch_trailer"

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
        void checkHexSize(const string& size);
        void setFirstSize();
        void setSize();
        void writeContent();
        void parseTrailer();
        void throwException(int code);
        // Stop writing chunk content to the destination file (an
        // upload or CGI-staged body being abandoned after an error)
        // without losing parse position - _buffer/_state/_chunkSize
        // stay exactly where they were, so the very next byte fed in
        // continues the same chunk-framing walk, just discarding
        // instead of writing.
        void discardFromNowOn();
};
