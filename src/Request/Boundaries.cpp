#include "../../inc/Boundaries.hpp"

Boundaries::Boundaries() : _isFileCreated(false), _outfile(NULL), _state(BD_START) { };

Boundaries::~Boundaries() { 
    if (_outfile)
    {
        _outfile->close();
        delete _outfile;
    }
};

void Boundaries::parseBoundaryStart()
{
    size_t pos = _buffer.find("\r\n\r\n");
    if (pos == string::npos)
        return;
    _bd_start = _buffer.substr(0, pos);
    _buffer.erase(0, pos + 4);
    checkStartBoundary();
}

void Boundaries::checkStartBoundary()
{
    size_t pos = _bd_start.find("\r\n");
    string startBound = _bd_start.substr(0, pos);
    if (startBound != _boundary)
        throw 400;
    // TODO: check headers and content type for extension
    _state = BD_CONTENT;
    writeBodyContent();
}
void Boundaries::writeBodyContent()
{
    // ! edge case
    // _buffer.append(_bd_helper);
    // if (_buffer.length() < _endBoundary.length())
    //     throw 400;
    string bound = "\r\n" + _boundary;
    size_t pos = _buffer.find(bound);
    if (pos == string::npos)
    {
        _outfile->write(_buffer.c_str(), _buffer.length());
        _outfile->flush();
        _buffer.clear();
    } else {
        if (bound + "--" == "\r\n" + _endBoundary)
        {
            _buffer.erase(pos);
            _outfile->write(_buffer.c_str(), _buffer.length());
            _outfile->flush();
            _state = BD_END;
            throw 201;
        }
        _outfile->write(_buffer.c_str(), pos);
        _outfile->flush();
        _state = BD_MID;
        checkMidBoundary();
    }
}

void Boundaries::checkMidBoundary()
{
    size_t pos = _buffer.find("\r\n\r\n");
    if (pos == string::npos)
        return;
    _bd_body = _buffer.substr(0, pos);
    _buffer.erase(0, pos + 4);
    _state = BD_CONTENT;
    writeBodyContent();
}

void Boundaries::parseBoundary(const string& buffer, const string& boundary)
{
    this->_buffer = buffer;
    this->_boundary = boundary;
    this->_endBoundary = this->_boundary + "--";
    if (!_isFileCreated)
    {
        _outfile = new fstream("bounds.txt", ios::out);
        if (!_outfile->is_open())
            throw 500;
    }
    while (_buffer.length() > 0)
    {
        if (_state == BD_START)
            parseBoundaryStart();
        else if (_state == BD_CONTENT)
            writeBodyContent();
        else if (_state == BD_MID)
            checkMidBoundary();
    }
    // throw 201;
}