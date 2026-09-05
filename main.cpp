#include "inc/Server.hpp"
#include <climits>
t_events ep;
map<string, int> socketMap;
string confDir;

static void resolveConfDir()
{
    char exePath[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    confDir = "conf/";
    if (len != -1)
    {
        exePath[len] = '\0';
        string path(exePath);
        size_t slash = path.find_last_of('/');
        if (slash != string::npos)
            confDir = path.substr(0, slash + 1) + "conf/";
    }
}

int main(int argc, char const *argv[])
{
    try
    {
        srand(time(NULL));
        resolveConfDir();
        ifstream conf;
        (argc == 1) ? conf.open((confDir + DEFAULT_CONF).c_str()) : (argc == 2) ? conf.open(argv[1])
                                                                     : throw Server::ServerException(USAGE);
        if (!conf.is_open())
            throw Server::ServerException(ERR "Unable to open file");
        string buff;
        getline(conf, buff, '\0');
        Webserver server;
        server.brackets(buff);
        for (size_t i = 0; i < server._servers.size(); i++)
            server[i].parseServer(buff);
        server.start();
    }
    catch (const exception &e)
    {      
        cerr << e.what() << '\n';
    }
}
