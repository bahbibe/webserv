#include "../../inc/Boundaries.hpp"

Boundaries::Boundaries() : _isFileCreated(false), _outfile(NULL), _state(BD_START), _filesCounter(0), _contentLength(0), _writedContent(0) { };

Boundaries::~Boundaries() { };


void Boundaries::setBoundaries(const string& boundary, const string& uploadPath, size_t contentLength)
{
    this->_boundary = boundary;
    this->_endBoundary = this->_boundary + "--";
    this->_uploadPath = uploadPath;
    this->_contentLength = contentLength;
}

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
    size_t pos = _bd_start.find("\r\n\r\n");
    if (pos == string::npos)
        return;
    string boundary = _bd_start.substr(0, _bd_start.find("\r\n"));
    if (boundary != _boundary)
        throwException(400);
    _state = BD_CONTENT;
    handleBoundaries();
}

void Boundaries::writeContent(string& buffer)
{
    if (_contentLength < buffer.length())
    {
        _outfile->write(buffer.c_str(), _contentLength);
        _outfile->flush();
        buffer.clear();
        throwException(201);
    }
    _outfile->write(buffer.c_str(), buffer.length());
    _outfile->flush();
    buffer.clear();
    _writedContent += buffer.length();
    _contentLength -= buffer.length();
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
        writeContent(_buffer);
    }
    else if (midBoundaryPos != string::npos && endBoundaryPos != string::npos && midBoundaryPos < endBoundaryPos)
    {
        string content = _buffer.substr(0, midBoundaryPos);
        _buffer.erase(0, midBoundaryPos);
        writeContent(content);
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
                    writeContent(content);
                    _buffer.erase(0, midBoundaryPos);
                    closeOutFile();
                } else {
                    string content = _buffer.substr(0, endBoundaryPos);
                    writeContent(content);
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
        writeContent(_buffer);
        closeOutFile();
        throwException(201);
    }
    else if (midBoundaryPos != string::npos)
    {
        _rest = _buffer.substr(midBoundaryPos);
        _buffer.erase(_buffer.length() - (_buffer.length() - midBoundaryPos));
        writeContent(_buffer);
        closeOutFile();
    }
}

void Boundaries::parseBoundary(const string& buffer, int readBytes)
{
    this->_buffer = "";
    this->_buffer.append(_rest);
    this->_buffer.append(buffer, 0, readBytes);
    if (!_isFileCreated)
        createFile();
    if (_state == BD_START)
        checkFirstBoundary();
    else 
        handleBoundaries();
}