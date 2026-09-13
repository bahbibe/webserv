#include "inc/Server.hpp"
#include <climits>
#include <spdlog/sinks/basic_file_sink.h>
t_events ep;
map<string, UniqueFd> socketMap;
string confDir;
string accessLogPath;
string pidPath;
string errorLogPath;
string errorLogLevel = "info";
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

// error_log <path> [level]; replaces the default stdout sink with a
// file sink, matching nginx's own error_log semantics (it redirects
// where diagnostics go, it doesn't add a second destination). Left
// alone (default stdout sink, "info" level) if error_log wasn't given -
// covers every existing invocation of this project unchanged. A file
// that can't be opened (bad path, no permission) falls back to the
// default sink with a warning rather than refusing to start over a
// logging misconfiguration.
static void setupLogging()
{
    if (errorLogPath.empty())
        return;
    try
    {
        auto fileSink = make_shared<spdlog::sinks::basic_file_sink_mt>(errorLogPath, false);
        auto logger = make_shared<spdlog::logger>("webserv", fileSink);
        spdlog::set_default_logger(logger);
    }
    catch (const spdlog::spdlog_ex &e)
    {
        spdlog::warn("Unable to open error_log at {}: {} - logging to stdout instead", errorLogPath, e.what());
        return;
    }
    spdlog::set_level(spdlog::level::from_str(errorLogLevel));
    // spdlog buffers by default; an error log sitting unflushed until
    // process exit (or worse, lost entirely on a signal that skips
    // spdlog's atexit flush) defeats the point of having one. Flush
    // every line as it's written, same as nginx's own error_log.
    spdlog::flush_on(spdlog::level::trace);
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
        parseGlobalDirectives(buff);
        for (size_t i = 0; i < server._servers.size(); i++)
            server[i].parseServer(buff);
        // Before setupSocket() below - it already logs ("Listening on
        // ...") as each socket binds, so error_log has to be wired up
        // first for that (and everything else) to land in the right
        // place. configErrors.report() a few lines down uses cerr
        // directly, not spdlog, so redirecting spdlog's sink this
        // early doesn't affect config-validation error reporting.
        setupLogging();
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
        if (!pidPath.empty())
        {
            ofstream pidFile(pidPath.c_str());
            if (pidFile.is_open())
                pidFile << getpid() << "\n";
            else
                spdlog::warn("Unable to write pid file at {}", pidPath);
        }
        server.start();
        if (!pidPath.empty())
            remove(pidPath.c_str());
    }
    catch (const exception &e)
    {
        spdlog::critical("{}", e.what());
        return 1;
    }
}
