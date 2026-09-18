# Security Policy

webserv is a from-scratch HTTP/1.1 + HTTPS server (see README) written
in C++ with raw sockets, epoll, and manual request parsing - the kind
of surface where memory-safety and parsing bugs are expected, not
hypothetical. This project has already fixed real issues found during
review, including two path-traversal bypasses (raw string-prefix
comparisons instead of real directory-boundary checks) and unchecked
`pipe()`/`fork()` return values in the CGI path.

## Reporting a vulnerability

This is a solo-maintained project, not a company with a security
team, so please keep that in mind when it comes to response time.

- Preferred: open a
  [GitHub Security Advisory](https://github.com/bahbibe/webserv/security/advisories/new)
  (private, only visible to the maintainer until resolved).
- Alternative: email boubkerahbibe@gmail.com.

Please don't open a public issue for a vulnerability before it's
fixed.

Include what you'd include for any bug report: the config/request
that triggers it, expected vs. actual behavior, and impact (crash,
memory disclosure, auth/privilege bypass, etc.). A minimal repro
(`curl` command, raw request bytes) is the fastest path to a fix.

## Scope

In scope: the `webserv` binary itself - request parsing, routing,
CGI, TLS handling, the config parser, privilege drop.

Out of scope: the example `WWW/` site content, the Docker image's
base OS packages (report those upstream to `debian`), and anything
that requires an attacker to already control the config file (config
is trusted input here, same as nginx/Apache's own model).

## Supported versions

Only the latest tagged release gets fixes. There's no long-term
support branch - this is a portfolio/learning project, not a
production-critical service with a maintenance SLA.
