#include "inc/Server.hpp"
#include "inc/ConfigParser.hpp"
#include "inc/Privileges.hpp"
#include <climits>
#include <spdlog/sinks/basic_file_sink.h>

// Global process state (ep, socketMap, confDir, g_shutdown, ...) lives
// in src/Globals.cpp, not here - see that file for why.

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

// Only called when no config file was given on the command line.
// Prefers a real system install (/etc/webserv/webserv.conf) over the
// binary-relative conf/default.conf fallback resolveConfDir() already
// set up - and, only in that case, also redirects confDir (so
// mime.types resolves from the same place - Server::mimeTypes() reads
// confDir + "mime.types") and the access log to /var/log/webserv/, a
// service binary living in /usr/local/sbin having no business writing
// access.log right next to itself.
//
// An explicit CLI config path never triggers any of this, regardless
// of whether /etc/webserv/ happens to exist on the machine - this
// function isn't even called in that case (see main() below).
// "Explicit argument always wins" has to mean everything downstream
// too, not just which config file gets read: this function used to be
// folded into resolveConfDir() (called unconditionally), which meant
// any machine that had /etc/webserv/ lying around - including this
// project's own tests/run_tests.sh, which always passes an explicit
// throwaway config - got its access log and mime.types silently
// redirected there anyway.
static string resolveDefaultConfigPath()
{
    if (access(SYSTEM_CONF_DIR, F_OK) == 0)
    {
        confDir = SYSTEM_CONF_DIR;
        accessLogPath = string(SYSTEM_LOG_DIR) + "access.log";
        return string(SYSTEM_CONF_DIR) + SYSTEM_CONF_FILE;
    }
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
        ConfigParser(buff).parse(server);
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
        // Strictly after every root-only startup step (sockets bound,
        // TLS certs loaded, pidfile written, logs open) and strictly
        // before server.start() - the point past which untrusted
        // request bytes get parsed.
        dropPrivileges();
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
