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
    // A real system install (see V3-PLAN.md) wins over the binary-
    // relative conf/ fallback above, whenever /etc/webserv/ actually
    // exists - checked once here so mime.types resolution and the
    // auto-selected config path (resolveDefaultConfigPath() below)
    // agree on the same answer. Every existing invocation of this
    // project (run from a git checkout, no system install present)
    // is unaffected - confDir stays exactly the binary-relative path
    // it always was.
    if (access(SYSTEM_CONF_DIR, F_OK) == 0)
        confDir = SYSTEM_CONF_DIR;
}

// Only used when no config file was given on the command line -
// prefers the installed system config over the dev/repo-checkout
// default, matching whichever tier resolveConfDir() picked for
// confDir above.
static string resolveDefaultConfigPath()
{
    if (confDir == SYSTEM_CONF_DIR)
        return string(SYSTEM_CONF_DIR) + SYSTEM_CONF_FILE;
    return confDir + DEFAULT_CONF;
}

int main(int argc, char const *argv[])
{
    try
    {
        srand(time(NULL));
        resolveConfDir();
        ifstream conf;
        (argc == 1) ? conf.open(resolveDefaultConfigPath().c_str()) : (argc == 2) ? conf.open(argv[1])
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
            for (size_t i = 0; i < server._servers.size(); i++)
                server[i].setupSsl();
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
        spdlog::critical("{}", e.what());
        return 1;
    }
}
