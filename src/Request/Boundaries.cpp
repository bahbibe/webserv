#include "../../inc/Boundaries.hpp"

Boundaries::Boundaries() : 
    _index(0), _state(BS_START),
    _isFileCreated(false), _outfile(NULL) { };

Boundaries::~Boundaries() { 
    if (_outfile)
    {
        _outfile->close();
        delete _outfile;
    }
};

void Boundaries::checkStartBoundary()
{
    _helperLine += _buffer[_index++];
    if (Helpers::checkLineEnd(_helperLine))
    {
        if (_helperLine.substr(0, _helperLine.length() - 2) == _boundary)
        {
            _helperLine.clear();
            _state = BS_HEADERS;
        }
        else
            throw 400;
    }
}

void Boundaries::checkBoundaryHeaders()
{
    _helperLine += _buffer[_index++]; 
    if (_helperLine == "\r\n")
    {
        _state = BS_BODY;
        return;
    }
    // TODO: Check headers for content type ...
    if (Helpers::checkLineEnd(_helperLine))
        _helperLine.clear();
}

void Boundaries::checkBoundaryBody()
{
    if (!_isFileCreated)
    {
        _outfile = new fstream("./bounds.txt", ios::out);
        if (!this->_outfile->is_open())
            throw 500;
        _isFileCreated = true;
    }
    if (Helpers::checkLineEnd(_helperLine))
    {
        _helperLine.clear();
        _state = BS_END;
        return;
    }
    _outfile->write(&_buffer[_index], 1);
    _outfile->flush();
    _helperLine += _buffer[_index++];
}

void Boundaries::checkBoundaryEnds()
{
    cout << YELLOW "END" RESET << endl;
    _helperLine += _buffer[_index++];
    throw 200;
    if (_helperLine == "\r\n")
        throw 200;
}

void Boundaries::parseBoundary(const string& buffer, const string& boundary)
{
    // TODO: read only the content length
    _index = 0;
    this->_helperLine.clear();
    this->_buffer = buffer;
    this->_boundary = boundary;
    while (_index < _buffer.size())
    {
        if (_state == BS_START)
            checkStartBoundary();
        else if (_state == BS_HEADERS)
            checkBoundaryHeaders();
        else if (_state == BS_BODY)
            checkBoundaryBody();
        else if (_state == BS_END)
            checkBoundaryEnds();
    }
}