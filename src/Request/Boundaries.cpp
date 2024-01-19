#include "../../inc/Boundaries.hpp"

Boundaries::Boundaries() : _isFileCreated(false), _outfile(NULL), _state(BD_START), _filesCounter(0) { };

Boundaries::~Boundaries() { };

// Boundaries::Boundaries(const Boundaries& other)
// {
//     *this = other;
// }

// Boundaries& Boundaries::operator=(const Boundaries& rhs)
// {
//     if (this != &rhs)
//     {
//         this->_buffer = rhs._buffer;
//         this->_boundary = rhs._boundary;
//         this->_endBoundary = rhs._endBoundary;
//         this->_isFileCreated = rhs._isFileCreated;
//         this->_outfile = rhs._outfile;
//         this->_uploadPath = rhs._uploadPath;
//         this->_state = rhs._state;
//         this->_bd_start = rhs._bd_start;
//         this->_rest = rhs._rest;
//         this->_filesCounter = rhs._filesCounter;
//     }
//     return *this;
// }

void Boundaries::throwException(int code)
{
    if (code != 201)
    {
        if (_outfile)
        {
            _outfile->close();
            delete _outfile;
            _outfile = NULL;
        }
    }
    throw code;
}

void Boundaries::createFile()
{
    if (_isFileCreated)
        return;
    // TODO: to get the extension later
    struct timeval tp;
    gettimeofday(&tp, NULL);
    long long ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;
    stringstream ss;
    ss << ms;
    string timestamp = ss.str();
    ss.str("");
    ss << _filesCounter++;
    string fileName = _uploadPath + "upload_" + timestamp + "_" + ss.str() + ".txt";
    _outfile = new fstream(fileName.c_str(), ios::out | ios::binary);
    if (!_outfile->is_open())
        throwException(500);
    _isFileCreated = true;
}

void Boundaries::checkFirstBoundary()
{
    // TODO: to handle later the case where the boundary at first buffer not found
    size_t pos = _buffer.find("\r\n\r\n");
    if (pos == string::npos)
        return;
    string boundary = _buffer.substr(0, _buffer.find("\r\n"));
    if (boundary != _boundary)
        throwException(400);
    _buffer.erase(0, pos + 4);
    _state = BD_CONTENT;
    handleBoundaries();
}

void Boundaries::writeContent()
{
    _outfile->write(_buffer.c_str(), _buffer.length());
    _outfile->flush();
    _buffer.clear();
}

void Boundaries::handleBoundaries()
{
    size_t midBoundaryPos = _buffer.find(_boundary);
    size_t endBoundaryPos = _buffer.find(_endBoundary);

    if (midBoundaryPos == string::npos && endBoundaryPos == string::npos)
    {
        if (_buffer.length() > _boundary.length())
        {
            _rest = _buffer.substr(_buffer.length() - _boundary.length());
            _buffer.erase(_buffer.length() - _boundary.length());
        }
        else
        {
            _rest = _buffer.substr(0, _buffer.length());
            _buffer.clear();
        }
        writeContent();
    }
    else if (endBoundaryPos != string::npos)
    {
        _buffer.erase(endBoundaryPos);
        writeContent();
        _isFileCreated = false;
        throwException(201);
    }
    else if (midBoundaryPos != string::npos)
    {
        _rest = _buffer.substr(midBoundaryPos);
        _buffer.erase(_buffer.length() - (_buffer.length() - midBoundaryPos));
        writeContent();
        _outfile->close();
        delete _outfile;
        _isFileCreated = false;
    }
}

void Boundaries::parseBoundary(const string& buffer, const string& boundary, int readBytes, const string& uploadPath)
{
    this->_boundary = boundary;
    this->_endBoundary = this->_boundary + "--";
    this->_buffer = "";
    this->_buffer.append(_rest);
    this->_buffer.append(buffer, 0, readBytes);
    this->_uploadPath = uploadPath;
    if (!_isFileCreated)
        createFile();
    if (_state == BD_START)
        checkFirstBoundary();
    else 
        handleBoundaries();
}