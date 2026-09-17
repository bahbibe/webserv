#include "../../inc/Chunks.hpp"

Chunks::Chunks() : _state(CH_START), _outfile(NULL), _chunkSize(0), _writedContent(0), _nextBufferSize(BUFFER_SIZE), _clientMaxBodySize(0) {};

Chunks::~Chunks() {};

void Chunks::setChunks(fstream *outfile, const string& filePath, size_t clientMaxBodySize)
{
    this->_outfile = outfile;
    this->_filePath = filePath;
    this->_clientMaxBodySize = clientMaxBodySize;
}

void Chunks::throwException(int code)
{
    if (code != 201 && _outfile)
        remove(_filePath.c_str());
    throw code;
}

void Chunks::discardFromNowOn()
{
    this->_outfile = NULL;
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
    _buffer.erase(0, pos + 2);
    if (_chunkSize == 0)
    {
        _state = CH_TRAILER;
        parseTrailer();
        return;
    }
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
    _buffer.erase(0, pos + 2);
    if (_chunkSize == 0)
    {
        _state = CH_TRAILER;
        parseTrailer();
        return;
    }
    _state = CH_CONTENT;
    writeContent();
}

// RFC 9112 6.1: chunked-body ends with the "0" last-chunk, then zero
// or more trailer field lines, then a final CRLF - not just the "0"
// line itself. Skipped over entirely before this change: the chunked
// parser considered the body done the instant it saw a zero-size
// chunk announcement, leaving that final CRLF (and any trailers)
// unread on the wire. Harmless when every chunked POST closed the
// connection right after anyway, but a real bug once chunked POSTs
// became keep-alive eligible - those leftover bytes would get
// misparsed as the start of the next request. Trailer field values
// themselves are discarded, not exposed anywhere - this project has
// no directive that reads them.
void Chunks::parseTrailer()
{
    while (true)
    {
        size_t pos = _buffer.find("\r\n");
        if (pos == string::npos)
            return;
        if (pos == 0)
        {
            _buffer.erase(0, 2);
            throwException(201);
        }
        _buffer.erase(0, pos + 2);
    }
}

void Chunks::writeContent()
{
    if (_buffer.empty())
        return;
    // client_max_body_size is a disk-usage guard on the destination
    // file - doesn't apply once discardFromNowOn() has been called
    // (nothing being written anymore); bounding how long a drain can
    // run is the idle/absolute request-timeout drain-abort mechanism
    // instead (see Webserver::start()'s periodic scan), same as the
    // Content-Length drain path.
    if (_outfile && _clientMaxBodySize > 0 && (_writedContent + _chunkSize > _clientMaxBodySize))
        throwException(413);
    if (_buffer.length() <= _chunkSize)
    {
        if (_outfile)
        {
            _outfile->write(_buffer.c_str(), _buffer.length());
            _outfile->flush();
        }
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
    else {
        string content = _buffer.substr(0, _chunkSize);
        if (_outfile)
        {
            _outfile->write(content.c_str(), content.length());
            _outfile->flush();
        }
        _buffer.erase(0, _chunkSize);
        _writedContent += _chunkSize;
        _chunkSize -= content.length();
        _chunkSize = 0;
        _state = CH_SIZE;
        setSize();
    }
}

int Chunks::parse(const string& buffer, int readBytes)
{
    // The "wait for a fresh '0\r\n' before processing an odd-sized
    // read" heuristic below only makes sense while still looking for
    // the last-chunk announcement. Once that's already been seen and
    // consumed (CH_TRAILER), there's no new "0\r\n" coming - only the
    // trailer part and the final CRLF - so this has to bypass the
    // heuristic and process whatever arrived immediately, or trailer
    // bytes arriving as their own small read would stall forever.
    if (_state != CH_TRAILER && _state.compare(CH_START) != 0 && readBytes != _nextBufferSize)
    {
        _helper.append(buffer, 0, readBytes);
        if (_helper.find("\r\n0\r\n") == string::npos)
            return _nextBufferSize;
    }
    _buffer.append(_helper);
    _buffer.append(buffer, 0, readBytes);
    if (_state == CH_START)
        setFirstSize();
    else if (_state == CH_SIZE)
        setSize();
    else if (_state == CH_CONTENT)
        writeContent();
    else if (_state == CH_TRAILER)
        parseTrailer();
    _helper.clear();
    return _nextBufferSize;
}