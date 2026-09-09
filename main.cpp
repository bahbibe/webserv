#include "inc/Server.hpp"
#include <climits>
t_events ep;
map<string, UniqueFd> socketMap;
string confDir;
string accessLogPath;
volatile sig_atomic_t g_shutdown = 0;
volatile sig_atomic_t g_reopenLog = 0;
ConfigValidator configErrors;

void handleShutdownSignal(int)
{
    g_shutdown = 1;
}

void handleReopenLogSignal(int)
{
    g_reopenLog = 1;
}

static void resolveConfDir()
{
    char exePath[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    string execDir = "./";
    confDir = "conf/";
    if (len != -1)
    {
        exePath[len] = '\0';
        string path(exePath);
        size_t slash = path.find_last_of('/');
        if (slash != string::npos)
            execDir = path.substr(0, slash + 1);
        confDir = execDir + "conf/";
    }
    accessLogPath = execDir + "access.log";
}

int main(int argc, char const *argv[])
{
    try
    {
        srand(time(NULL));
        resolveConfDir();
        ifstream conf;
        (argc == 1) ? conf.open((confDir + DEFAULT_CONF).c_str()) : (argc == 2) ? conf.open(argv[1])
                                                                     : throw WebservException(USAGE);
        if (!conf.is_open())
            throw WebservException(ERR "Unable to open file");
        string buff;
        getline(conf, buff, '\0');
        Webserver server;
        server.brackets(buff);
        for (size_t i = 0; i < server._servers.size(); i++)
            server[i].parseServer(buff);
        if (!configErrors.hasErrors())
        {
            for (size_t i = 0; i < server._servers.size(); i++)
                server[i].setupSocket();
        }
        if (configErrors.hasErrors())
        {
            configErrors.report(cerr);
            return 1;
        }
        server.start();
    }
    catch (const exception &e)
    {
        cerr << e.what() << '\n';
        return 1;
    }
}
