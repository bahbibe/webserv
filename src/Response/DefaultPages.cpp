#include "../../inc/Response.hpp"

// Content the server generates itself when there's no matching static
// resource to serve: directory autoindex listings and synthesized
// default error pages (used when no error_page directive configures a
// real file for the status code in question).

void Response::checkAutoInedx(Request &request)
{
        for (size_t i = 0; i < request.directives.indexs.size(); i++)
        {
            string index = this->_path + request.directives.indexs[i];
            this->file.open(index.c_str(), ios::in | ios::binary);
            if (file.is_open())
            {
                this->_path = index;
                if (request.directives.isCgiAllowed && (this->_path.rfind(".php") != string::npos || this->_path.rfind(".py") != string::npos))
                {
                    this->_absPath = this->_path;
                    file.close();
                    this->_cgiPath = resolveCgiPath(request);
                   this->_cgiAutoIndex = true;
                }
                else
                {
                    this->_statusCode = 200;
                    this->_flag = true;
                    findeContentType(request);
                    SendHeader();
                }
                return;
            }
        }
    if (request.directives.autoindex)
        tree_dir();
    else
    {
        request.isErrorCode = 1;
        this->_statusCode = 403;
        checkErrors(request);
    }
}

void Response::tree_dir()
{
    DIR *dir = opendir(this->_path.c_str());
    if (dir)
    {
        this->_contentType = "text/html";
        SendHeader();
        if (this->_body.empty())
        {
            struct dirent *dp;
            string name;
            string body = "<html><head></head><body><ul>";
            while ((dp = readdir(dir)))
            {
                name = dp->d_name;
                body += "<li><a href='"+ name  +"'>"  + name +"</a></li>";
            }
            stringstream ss;
            ss << hex << body.length();
            this->_body = ss.str() + "\r\n";
            this->_body += body + "\r\n";
            this->_body += "0\r\n\r\n";
            this->_bodyOffset = 0;
            this->_pendingBodyLen = body.length();
        }
        closedir(dir);
        if (this->_isHead)
        {
            this->_defaultError = true;
            this->_isfinished = true;
            this->_flag = false;
        }
        else if (flushBody())
        {
            this->_bytesSent += this->_pendingBodyLen;
            this->_defaultError = true;
            this->_isfinished = true;
            this->_flag = false;
        }
    }
}

string Response::getErrorPage(Request &request, int statusCode)
{
    map<string, string>::iterator it;
    it = request.directives.errorPages.find(toSting(statusCode));
    if (it != request.directives.errorPages.end())
        return it->second;
    return "default";
}

void Response::checkErrors(Request &request)
{
    this->_path = getErrorPage(request, this->_statusCode);
    if (!this->_flag)
        file.open(this->_path.c_str(), ios::in | ios::binary);
    if (!file.is_open() || this->_path == "default")
    {
        this->_contentType = "text/html";
        SendHeader();
        if (this->_body.empty())
        {
            string error;
            stringstream ss;
            map<int, string>::iterator it;
            it = this->status.find(this->_statusCode);
            error = templateError(it != this->status.end() ? it->second : "500 Internal Server Error");
            ss << hex << error.length();
            this->_body = ss.str() + "\r\n";
            this->_body += error + "\r\n";
            this->_body += "0\r\n\r\n";
            this->_bodyOffset = 0;
            this->_pendingBodyLen = error.length();
        }
        if (this->_isHead)
        {
            this->_defaultError = true;
            this->_isfinished = true;
        }
        else if (flushBody())
        {
            this->_bytesSent += this->_pendingBodyLen;
            this->_defaultError = true;
            this->_isfinished = true;
        }
    }
    else if (file.good() && !this->_flag)
    {
        findeContentType(request);
        SendHeader();
        this->_flag = true;
    }

}

string Response::templateError(string errorType)
{
    string errorBody;

    errorBody = "<html><head><title>"+ errorType +"</title></head>";
    errorBody += "<body><center><h1>"+ errorType + "</h1></center><hr><center>M0BLACK</center></body>";
    return errorBody;
}
