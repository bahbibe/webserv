#include "../../inc/Response.hpp"

double Response::fileSize(string path)
{
    struct stat metadata;
    stat(path.c_str(), &metadata);
    return metadata.st_size;
}

string Response::resolveCgiPath(Request &request) const
{
    string ext = (this->_path.rfind(".php") != string::npos) ? "php" : "py";
    map<string, string>::const_iterator it = request.directives.cgiPaths.find(ext);
    if (it != request.directives.cgiPaths.end())
        return it->second;
    return ext == "php" ? "/usr/bin/php-cgi" : "/usr/bin/python3";
}

void Response::CGI(Request &req)
{
    if (this->_isCGI == false)
    {
        this->_isCGI = true;
        this->start = time(NULL);
        this->_randPath = "/tmp/" + toSting(rand());
        fillEnv(req);
        cout.flush();
        this->pid = fork();
        if (this->pid == 0)
        {
            const char *argv[] = {this->_cgiPath.c_str(), this->_absPath.c_str() ,NULL};
            freopen(this->_randPath.c_str(), "w", stdout);
            if (this->_method == "GET")
                close(0);
            else
                freopen(req.directives.cgiFileName.c_str(), "r", stdin);
            size_t slash = this->_absPath.rfind('/');
            if (slash != string::npos)
                chdir(this->_absPath.substr(0, slash).c_str());
            execve(argv[0], (char* const*)argv, this->env);
            cerr << "execve: " << strerror(errno) << "\n";
            exit(127);
        }
    }
    int status = 0;
    pid_t wPid = waitpid(pid, &status, WNOHANG);
    double elapsed = CLOCKWORK(this->start);
    if (wPid == -1  || wPid > 0 || elapsed > CGI_TIMEOUT)
    {
        char buffer[1024] = {0};
        unsigned long pos;
        string str;
        stringstream ss;

        this->_cgiAutoIndex = false;
        this->_path = this->_randPath;
        this->file.open(this->_path.c_str(), ios::in | ios::binary);
        this->_flag = true;
        this->_isCGI = true;
        if (status != 0 || elapsed > CGI_TIMEOUT)
        {
            this->_isErrorCode = true;
            if (status != 0)
                this->_statusCode = 500;
            else
            {
                kill(this->pid, SIGKILL);
                waitpid(pid, 0, 0);
                this->_statusCode = 504;
            }
            file.close();
            if (this->_isCGI == true)
            {
                freeEnv(this->env);
                this->env = NULL;
                remove(this->_path.c_str());
                remove(req.directives.cgiFileName.c_str());
            }
            checkErrors(req);
        }
        else
        {
            if (file.is_open())
            {
                file.read(buffer, 1023);
                ss << buffer;
                str = ss.str();
                if ((pos = str.find("\r\n\r\n")) != string::npos)
                {
                    int s = (str.length() - (pos + 4)) * -1;
                    this->_cgiHeader = str.substr(0, pos + 2);
                    this->file.clear();
                    this->file.seekg(s , ios::cur);
                }
                else
                {
                    file.clear();
                    file.seekg(0 ,ios::beg);
                }
            }
            this->_statusCode = 200;
            this->_method = "GET";
            SendHeader();
            if (headerSent())
                GET(req);
        }
    }
}

int Response::fillEnv(Request &req)
{
    vector<string> vars;
    string requestUri = req.directives.requestTarget;
    if (!req.directives.queryString.empty())
        requestUri += "?" + req.directives.queryString;

    vars.push_back("REQUEST_METHOD=" + this->_method);
    vars.push_back("QUERY_STRING=" + req.directives.queryString);
    vars.push_back("REDIRECT_STATUS=200");
    vars.push_back("PATH_INFO=");
    vars.push_back("SCRIPT_FILENAME=" + this->_absPath);
    vars.push_back("SCRIPT_NAME=" + req.directives.requestTarget);
    vars.push_back("REQUEST_URI=" + requestUri);
    vars.push_back("GATEWAY_INTERFACE=CGI/1.1");
    vars.push_back("SERVER_PROTOCOL=" + req.getHttpVersion());
    vars.push_back("SERVER_SOFTWARE=webserv/1.0");
    vars.push_back("REMOTE_ADDR=" + req._clientIp);
    vars.push_back("CONTENT_TYPE=" + req.directives.contentType);
    if (req.getServer())
    {
        vars.push_back("SERVER_NAME=" + req.getServer()->getHost());
        vars.push_back("SERVER_PORT=" + req.getServer()->getPort());
    }
    if (this->_method == "GET" || this->_method == "HEAD")
        vars.push_back("CONTENT_LENGTH=0");
    else
    {
        double size = fileSize(req.directives.cgiFileName);
        vars.push_back("CONTENT_LENGTH=" + toSting(size));
    }
    map<string, string> headers = req.getHeaders();
    for (map<string, string>::iterator it = headers.begin(); it != headers.end(); ++it)
    {
        string name = it->first;
        if (name == "content-type" || name == "content-length")
            continue;
        string envName = "HTTP_";
        for (size_t i = 0; i < name.length(); i++)
            envName += (name[i] == '-') ? '_' : (char)toupper((unsigned char)name[i]);
        vars.push_back(envName + "=" + it->second);
    }

    this->env = new char *[vars.size() + 1];
    for (size_t i = 0; i < vars.size(); i++)
        env[i] = dupCStr(vars[i].c_str());
    env[vars.size()] = NULL;
    return 1;
}

char *Response::dupCStr(const char *s) const
{
    size_t len = strlen(s);
    char *copy = new char[len + 1];
    memcpy(copy, s, len + 1);
    return copy;
}

void Response::freeEnv(char **env)
{
    for (int i = 0; env[i]; i++)
        delete[] env[i];
    delete[] env;
}

char **Response::dupEnv(char * const *env) const
{
    if (!env)
        return NULL;
    int n = 0;
    while (env[n])
        n++;
    char **copy = new char *[n + 1];
    for (int i = 0; i < n; i++)
        copy[i] = dupCStr(env[i]);
    copy[n] = NULL;
    return copy;
}
