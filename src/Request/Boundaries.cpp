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

void Boundaries::setMimeTypes(map<string, vector<string> > mimeTypes)
{
    this->_mimeTypes = mimeTypes;
}

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

string Boundaries::getExtension()
{
    size_t pos = _bd_start.find("Content-Type: ");
    if (pos == string::npos)
        return ".txt";
    string contentType = _bd_start.substr(pos + 14);
    pos = contentType.find("\r\n");
    if (pos == string::npos)
        return ".txt";
    contentType.erase(pos);
    pos = contentType.find(";");
    if (pos != string::npos)
        contentType.erase(pos);
    map<string, vector<string> >::iterator it = _mimeTypes.find(contentType);
    if (it != _mimeTypes.end())
    {
        vector<string> extensions = it->second;
        if (extensions.size() > 0)
            return string("." + extensions[0]);
    } 
    return ".txt";
}

void Boundaries::createFile()
{
    if (_isFileCreated)
        return;
    size_t pos = _buffer.find("\r\n\r\n");
    if (pos == string::npos)
        return;
    _bd_start = _buffer.substr(0, pos + 4);
    _buffer.erase(0, pos + 4);
    struct timeval tp;
    gettimeofday(&tp, NULL);
    long long ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;
    stringstream ss;
    ss << ms;
    string timestamp = ss.str();
    ss.str("");
    ss << _filesCounter++;
    string fileName = _uploadPath + "upload_" + timestamp + "_" + ss.str() + getExtension();
    _outfile = new fstream(fileName.c_str(), ios::out | ios::binary);
    if (!_outfile->is_open())
        throwException(500);
    _isFileCreated = true;
}

void Boundaries::checkFirstBoundary()
{
    // TODO: to handle later the case where the boundary at first buffer not found
    size_t pos = _bd_start.find("\r\n\r\n");
    if (pos == string::npos)
        return;
    string boundary = _bd_start.substr(0, _bd_start.find("\r\n"));
    if (boundary != _boundary)
        throwException(400);
    // _bd_start.erase(0, pos + 4);
    _state = BD_CONTENT;
    handleBoundaries();
}

void Boundaries::writeContent()
{
    _outfile->write(_buffer.c_str(), _buffer.length());
    _outfile->flush();
    _buffer.clear();
}

void Boundaries::closeOutFile()
{
    _outfile->close();
    delete _outfile;
    _isFileCreated = false;
    _outfile = NULL;
}

void Boundaries::handleBoundaries()
{
    // TODO: check if the content is too big than maxBodyClientSize
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
    else if (midBoundaryPos != string::npos && endBoundaryPos != string::npos && midBoundaryPos < endBoundaryPos)
    {
        string content = _buffer.substr(0, midBoundaryPos);
        _buffer.erase(0, midBoundaryPos);
        _outfile->write(content.c_str(), content.length());
        _outfile->flush();
        closeOutFile();
        while (1)
        {
            if (!_isFileCreated)
                createFile();
            else
            {
                midBoundaryPos = _buffer.find(_boundary);
                endBoundaryPos = _buffer.find(_endBoundary);
                if (midBoundaryPos != string::npos && midBoundaryPos < endBoundaryPos)
                {
                    string content = _buffer.substr(0, midBoundaryPos);
                    _outfile->write(content.c_str(), content.length());
                    _outfile->flush();
                    _buffer.erase(0, midBoundaryPos);
                    closeOutFile();
                } else {
                    string content = _buffer.substr(0, endBoundaryPos);
                    _outfile->write(content.c_str(), content.length());
                    _outfile->flush();
                    _buffer.erase(0, endBoundaryPos);
                    closeOutFile();
                    throwException(201);
                }
            }
        }
    }
    else if (endBoundaryPos != string::npos)
    {
        _buffer.erase(endBoundaryPos);
        writeContent();
        closeOutFile();
        throwException(201);
    }
    else if (midBoundaryPos != string::npos)
    {
        _rest = _buffer.substr(midBoundaryPos);
        _buffer.erase(_buffer.length() - (_buffer.length() - midBoundaryPos));
        writeContent();
        closeOutFile();
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