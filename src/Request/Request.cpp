#include "../../inc/Request.hpp"

Request::~Request()
{
    if (this->_outfileIsCreated)
    {
        this->_outfile->close();
        delete this->_outfile;
    }
}

Request::Request() : _lineCount(0), _statusCode(200), isRequestFinished(false), _isFoundCRLF(false), _outfile(NULL), _outfileIsCreated(false)
{
    memset(_buffer, 0, BUFFER_SIZE);
    this->_uploadFilePath = "upload/";
}

void Request::readRequest(int socket)
{
    this->_socketFd = socket;
    int readBytes = read(_socketFd, _buffer, BUFFER_SIZE);
    if (readBytes == -1)
        throw Server::ServerException(ERR "Failed to read from socket");
    _buffer[readBytes] = '\0';
    this->parseRequest(_buffer);
    if (!this->isRequestFinished)
        return;
    map<string, string>::iterator it = _headers.find("host");
    if (it == _headers.end() || it->second.length() == 0)
        setStatusCode(400, "No Host Header");
    if (_method != "POST")
        setStatusCode(200, "OK");
    if (_headers.find("transfer-encoding") != _headers.end() && _headers["transfer-encoding"] != "chunked")
        setStatusCode(501, "Unsupported Transfer-Encoding");
    if (_headers.find("content-length") != _headers.end() && _headers.find("transfer-encoding") != _headers.end())
        setStatusCode(400, "Both Content-Length and Transfer-Encoding are present");
    this->setStatusCode(200, "OK");
}

typedef pair<map<string, string>::iterator, bool> ret_type;

void Request::parseRequest(string buffer)
{
    while (buffer.length() > 0)
    {
        if (this->_lineCount == 0)
            this->parseRequestLine(buffer);
        else
        {
            size_t pos = buffer.find("\r\n");
            if (pos == string::npos)
                break;
            string header = buffer.substr(0, pos);
            buffer.erase(0, pos + 2);
            if (header.length() == 0 && buffer.length() != 0)
                return this->parseBody(buffer);
            if (header.length() != 0)
            {
                vector<string> headerTokens = this->split(header, ": ");
                string headerName = toLowerCase(headerTokens[0]);
                string headerValue = headerTokens.size() > 1 ? headerTokens[1] : "";
                trim(headerValue);
                ret_type ret = this->_headers.insert(pair<string, string>(headerName, headerValue));
                //! NOTE: random headers can be duplicated
                if (ret.second == false)
                    setStatusCode(400, "Duplicate Header");
            }
        }
    }
}

void Request::parseRequestLine(string& buffer)
{
    size_t pos = buffer.find("\r\n");
    if (pos == string::npos)
        this->setStatusCode(400, "Invalid Request Line");
    string requestLine = buffer.substr(0, pos);
    buffer.erase(0, pos + 2);
    vector<string> tokens = this->split(requestLine, " ");
    if (tokens.size() != 3)
        this->setStatusCode(400, "Invalid Request Line");
    this->_method = tokens[0];
    this->_requestTarget = tokens[1];
    this->_httpVersion = tokens[2];
    // TODO: 405 Method Not Allowed
    if (this->_method != "GET" && this->_method != "POST" && this->_method != "DELETE")
        setStatusCode(400, "Invalid Method");
    // TODO: validate the request target
    
    if (this->_httpVersion.length() != 8 || this->_httpVersion.substr(0, 5) != "HTTP/")
        setStatusCode(400, "Invalid HTTP Version");
    else if (this->_httpVersion.substr(5, 3) != "1.1")
        setStatusCode(505, "HTTP Version Not Supported");
    this->_lineCount++;
}

void Request::createOutfile()
{
    // TODO: upload the file in the upload folder
    this->_filePath = this->_uploadFilePath + "file.txt";
    this->_outfile = new fstream(this->_filePath.c_str(), ios::out);
    if (!this->_outfile->is_open())
        setStatusCode(500, "Failed to create file");
    this->_outfileIsCreated = true;
}

void Request::parseBody(string buffer)
{
    if (this->_method != "POST")
        return;
    if (!this->_outfileIsCreated)
        this->createOutfile();
    if (_headers.find("content-length") != _headers.end())
    {
        string contentLengthStr = _headers["content-length"];
        for (size_t i = 0; i < contentLengthStr.length(); i++)
            if (!isdigit(contentLengthStr[i]))
                setStatusCode(400, "Invalid Content-Length");
        size_t contentLength = atoi(_headers["content-length"].c_str());
        if (contentLength == 0)
            setStatusCode(200, "OK");
        if (buffer.length() < contentLength)
        {
            // TODO: read more from socket
            cout << "Not enough data in buffer: Should read more" << endl;
            // this->_body = buffer;
            return;
        }
        if (buffer.length() > contentLength)
            buffer.erase(contentLength, buffer.length() - contentLength);
        this->_outfile->write(buffer.c_str(), contentLength);
        this->_outfile->flush();
        buffer.erase(0, contentLength);
        if (buffer.length() == 0)
            setStatusCode(201, "Created");
    }
}

void Request::setStatusCode(int statusCode, string statusMessage)
{
    this->printRequest();
    this->_statusCode = statusCode;
    this->isRequestFinished = true;
    stringstream ss;
    ss << statusCode;
    this->_statusMessage = statusCode >= 400 ? RED + statusMessage + ": " + ss.str() + RESET : GREEN + statusMessage + ": " + ss.str() + RESET;
    throw  statusCode;
}

void Request::printRequest()
{
    cout << "Method: " << _method << endl;
    cout << "Request Target: " << _requestTarget << endl;
    cout << "HTTP Version: " << _httpVersion << endl;
    cout << "Headers size: " << _headers.size() << endl;
    cout << "Headers: " << endl;
    map<string, string>::iterator it = _headers.begin();
    for (; it != _headers.end(); it++)
        cout << it->first << ": " << it->second << endl;
}

bool Request::getIsRequestFinished() const
{
    return this->isRequestFinished;
}

string Request::getStatusMessage() const
{
    return this->_statusMessage;
}

string Request::getMethod() const
{
    return this->_method;
}

string Request::getRequestTarget() const
{
    return this->_requestTarget;
}

string Request::getHttpVersion() const
{
    return this->_httpVersion;
}

map<string, string> Request::getHeaders() const
{
    return this->_headers;
}

fstream *Request::getOutFile() const
{
    return this->_outfile;
}

vector<string> Request::split(string str, string delimiter)
{
    vector<string> tokens;
    size_t pos = str.find(delimiter);
    while (pos != string::npos)
    {
        tokens.push_back(str.substr(0, pos));
        str.erase(0, pos + delimiter.length());
        pos = str.find(delimiter);
    }
    tokens.push_back(str);
    return tokens;
}

string Request::toLowerCase(const string &str)
{
    string lowerCaseStr = "";
    for (size_t i = 0; i < str.length(); i++)
        lowerCaseStr += tolower(str[i]);
    return lowerCaseStr;
}

void Request::trim(string& str)
{
    while (str.length() > 0 && str[0] == ' ')
        str.erase(0, 1);
    while (str.length() > 0 && str[str.length() - 1] == ' ')
        str.erase(str.length() - 1, 1);
}