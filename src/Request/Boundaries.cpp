#include "../../inc/Boundaries.hpp"

Boundaries::Boundaries() : _isFileCreated(false), _outfile(NULL), _state(BD_START) { 
    // _outfile = new fstream("bounds.txt");
    // if (!_outfile->is_open())
    //     throw 500;
};

Boundaries::~Boundaries() { 
    if (_outfile)
    {
        _outfile->close();
        delete _outfile;
    }
};

void Boundaries::checkStartBoundary()
{
    size_t pos = _bd_helper.find("\r\n");
    string startBound = _bd_helper.substr(0, pos);
    if (startBound != _boundary)
        throw 400;
    // TODO: check headers and content type for extension
}

void Boundaries::checkEndBoundary()
{
    // TODO: check this case later
    // if (_helper.find(_endBoundary) != string::npos)
    //     throw 201;
    // _outfile->write(_helper.c_str(), _helper.length());
    // _outfile->flush();
}

void Boundaries::writeBodyContent()
{
    
}

void Boundaries::parseBoundary(const string& buffer, const string& boundary)
{
    // // TODO: read only the content length
    this->_buffer = buffer;
    this->_boundary = boundary;
    this->_endBoundary = this->_boundary + "--";
    if (_state == BD_START)
    {
        string startBound = _buffer.substr(0, _buffer.find("\r\n\r\n"));
    }
    throw 201;
}