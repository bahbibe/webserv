#include "../../inc/Privileges.hpp"
#include <spdlog/spdlog.h>

void dropPrivileges()
{
    if (dropUser.empty())
        return;

    if (geteuid() != 0)
    {
        spdlog::warn("user directive is set but the process isn't running as root - staying as the launching account (uid {})", getuid());
        return;
    }

    struct passwd *pw = getpwnam(dropUser.c_str());
    if (!pw)
        throw WebservException(ERR "user: account vanished before startup: " + dropUser);

    gid_t targetGid = pw->pw_gid;
    if (!dropGroup.empty())
    {
        struct group *gr = getgrnam(dropGroup.c_str());
        if (!gr)
            throw WebservException(ERR "user: group vanished before startup: " + dropGroup);
        targetGid = gr->gr_gid;
    }

    // Order matters: initgroups() first, while still root, clears the
    // supplementary group list inherited from root (easy to miss, and
    // a real privilege-escalation vector if skipped - setgid()/
    // setuid() alone never touch supplementary groups). Then gid,
    // then uid - once the uid is dropped, changing gid may no longer
    // be permitted.
    if (initgroups(dropUser.c_str(), targetGid) != 0)
        throw WebservException(ERR "user: initgroups failed: " + string(strerror(errno)));
    if (setgid(targetGid) != 0)
        throw WebservException(ERR "user: setgid failed: " + string(strerror(errno)));
    if (setuid(pw->pw_uid) != 0)
        throw WebservException(ERR "user: setuid failed: " + string(strerror(errno)));

    // A process that dropped privileges correctly can never regain
    // root. If setuid(0) here doesn't fail, the drop didn't fully
    // take - the classic case being a leftover *saved* uid still 0,
    // which lets the process re-escalate via setuid() even without
    // CAP_SETUID. Hard failure, not a warning: the whole point of
    // this feature is that the running process is genuinely
    // unprivileged, not just configured to look that way.
    if (setuid(0) == 0)
        throw WebservException(ERR "user: privilege drop did not take effect (setuid(0) unexpectedly succeeded)");

    spdlog::info("Dropped privileges to user '{}' (uid {}, gid {})", dropUser, pw->pw_uid, targetGid);
}
