#pragma once
#include "webserv.hpp"

// Drops from root to the account named by the `user <name> [group];`
// directive (dropUser/dropGroup, already config-validated by
// ConfigParser::parseMainDirective - see inc/webserv.hpp), called
// once from main() after every root-only startup step (socket binds,
// TLS cert loads, pidfile write, log open) and strictly before
// server.start() - the point past which untrusted request bytes get
// parsed.
//
// No-op if `user` wasn't set. Soft-fails (logs a warning, keeps
// running as the launching account) if the process isn't root to
// begin with, since setuid()/setgid() would just fail with EPERM
// anyway and that's already no less privileged than the alternative -
// a normal dev/test scenario, not a misconfiguration. Throws
// WebservException (hard startup failure) if the process is root and
// the drop itself fails for any reason - silently continuing to run
// as root after the admin explicitly asked not to would be a real
// regression of exactly what this exists to fix.
void dropPrivileges();
