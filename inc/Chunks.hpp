#pragma once

#include "webserv.hpp"
#include "Server.hpp"
#include "Helpers.hpp"

#define CH_SIZE "ch_size"
#define CH_CONTENT "ch_content"

class Chunks {
    private:
        string _buffer;
        string _state;
        string _helper;

        fstream *_outfile;
        size_t _chunkSize;
        size_t _writedContent;
    
    public:
        Chunks();
        ~Chunks();

        void parse(const string& buffer, fstream *outfile);
        void setSize();
        void writeContent();
};
