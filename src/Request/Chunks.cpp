// #includ../e "../inc/Multiplexer.hpp"
#include "../../inc/Chunks.hpp"

Chunks::Chunks() : _state(CH_START), _outfile(NULL), _chunkSize(0), _writedContent(0), _nextBufferSize(BUFFER_SIZE) {};

Chunks::~Chunks() {};

void Chunks::throwException(int code)
{
    if (code != 201)
        remove(_filePath.c_str());
    throw code;
}

void Chunks::checkHexSize(const string& size)
{
    for (size_t i = 0; i < size.length(); i++)
    {
        if (!isxdigit(size[i]))
            throwException(400);
    }
}

void Chunks::setFirstSize()
{
    size_t pos = _buffer.find("\r\n");
    if (pos == string::npos)
        throwException(400);
    string size = _buffer.substr(0, pos);
    checkHexSize(size);
    _chunkSize = strtol(size.c_str(), NULL, 16);
    if (_chunkSize == 0)
        throwException(201);
    _buffer.erase(0, pos + 2);
    _writedContent = 0;
    _state = CH_CONTENT;
    writeContent();
}

void Chunks::setSize()
{
    _state = CH_SIZE;
    if (_buffer.empty())
        return;
    size_t pos = _buffer.find("\r\n");
    _buffer.erase(0, pos + 2);
    pos = _buffer.find("\r\n");
    string size = _buffer.substr(0, pos);
    checkHexSize(size);
    _chunkSize = strtol(size.c_str(), NULL, 16);
    if (_chunkSize == 0)
        throwException(201);
    _buffer.erase(0, pos + 2);
    _writedContent = 0;
    _state = CH_CONTENT;
    writeContent();
}

void Chunks::writeContent()
{
    if (_buffer.empty())
        return;
    if (_buffer.length() <= _chunkSize)
    {
        _outfile->write(_buffer.c_str(), _buffer.length());
        _outfile->flush();
        _chunkSize -= _buffer.length();
        _writedContent += _buffer.length();
        _buffer.erase(0, _buffer.length());
        if (_chunkSize == 0)
        {
            _state = CH_SIZE;
            _nextBufferSize = BUFFER_SIZE;
        }
        else if (_chunkSize < BUFFER_SIZE)
            _nextBufferSize = _chunkSize;
        else
            _nextBufferSize = BUFFER_SIZE;
    }
    // TODO: this the case when the chunk size is set to 0 and the next chunk is 0
    else {
        string content = _buffer.substr(0, _chunkSize);
        _outfile->write(content.c_str(), content.length());
        _outfile->flush();
        _buffer.erase(0, _chunkSize);
        _writedContent += _chunkSize;
        _chunkSize -= content.length();
        _chunkSize = 0;
        _state = CH_SIZE;
        setSize();
    }

}

int Chunks::parse(const string& buffer, fstream *outfile, const string& filePath, int readBytes)
{
    this->_outfile = outfile;
    this->_filePath = filePath;
    _buffer.append(buffer, 0, readBytes);
    if (_state == CH_START)
        setFirstSize();
    else if (_state == CH_SIZE)
        setSize();
    else if (_state == CH_CONTENT)
        writeContent();
    return _nextBufferSize;
}