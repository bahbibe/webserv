#include "../../inc/Chunks.hpp"

Chunks::Chunks() : _state(CH_SIZE), _outfile(NULL), _chunkSize(0), _writedContent(0) {};

Chunks::~Chunks() {};

void Chunks::setSize()
{
    size_t pos = _buffer.find("\r\n");
    if (pos == string::npos)
        throw 400;
    string size = _buffer.substr(0, pos);
    for (size_t i = 0; i < size.length(); i++)
        if (!isxdigit(size[i]))
            throw 400;
    _chunkSize = strtol(size.c_str(), NULL, 16);
    if (_chunkSize == 0)        
        throw 201;
    _buffer.erase(0, pos + 2);
    _state = CH_CONTENT;
}

void Chunks::writeContent()
{
    if (_buffer.length() <= _chunkSize)
    {
        _outfile->write(_buffer.c_str(), _buffer.length());
        _outfile->flush();
        _chunkSize -= _buffer.length();
        _writedContent += _buffer.length();
        _buffer.erase(0, _buffer.length());
        if (_chunkSize == _writedContent)
            _state = CH_SIZE;
    } else
    {
        string content = _buffer.substr(0, _chunkSize);
        _outfile->write(content.c_str(), content.length());
        _outfile->flush();
        _buffer.erase(0, _chunkSize);
        if (_buffer.substr(0, 2) != "\r\n")
            throw 400;
        _buffer.erase(0, 2);
        setSize();
    }
}

void Chunks::parse(const string& buffer, fstream *outfile)
{
    this->_outfile = outfile;
    this->_buffer = buffer;
    while (_buffer.length() > 0)
    {
        if (_state == CH_SIZE)
            setSize();
        else if (_state == CH_CONTENT)
            writeContent();
    }
}