#include "../inc/Server.hpp"

// Cross-cutting process state, defined once here so both the real
// webserv binary and the unit test binary (see V4-PLAN.md Phase 1)
// can link the rest of the codebase without pulling in main.cpp's own
// int main() - a translation unit can only have one of those.
t_events ep;
map<string, UniqueFd> socketMap;
string confDir;
string accessLogPath;
string pidPath;
string errorLogPath;
string errorLogLevel = "info";
string dropUser;
string dropGroup;
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
