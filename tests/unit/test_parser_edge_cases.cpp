// Edge-case tests for the current (pre-rewrite) config parser (see
// V4-PLAN.md Phase 2). Written *before* Phase 3-4 touch any parsing
// code, against the parser exactly as Phase 1 characterized it. Some
// of these are expected to fail right now - real, previously-
// unverified gaps, marked with doctest::may_fail(true) so the suite
// stays green while still surfacing them, and documented as gaps
// Phase 4's rewrite has to actually close (its own acceptance bar is
// every one of these passing for real, may_fail removed).

#include <doctest/doctest.h>
#include "ParserFixture.hpp"

// --- malformed structure ---

TEST_CASE_FIXTURE(ParserFixture, "an unclosed location block throws")
{
    string conf = "server {\n"
                  "    host 127.0.0.1\n"
                  "    listen 8080\n"
                  "    root " + rootPath + "\n"
                  "    location /foo {\n"
                  "        allow GET\n"
                  "}\n";
    // Only one closing brace above - closes the location, not the
    // server, so the server itself is left unclosed.

    Webserver server;
    CHECK_THROWS_AS(server.brackets(conf), WebservException);
}

TEST_CASE_FIXTURE(ParserFixture, "server brace on the line after the server keyword throws")
{
    string conf = "server\n"
                  "{\n"
                  "    host 127.0.0.1\n"
                  "    listen 8080\n"
                  "    root " + rootPath + "\n"
                  "}\n";

    Webserver server;
    CHECK_THROWS_AS(server.brackets(conf), WebservException);
}

TEST_CASE_FIXTURE(ParserFixture, "location brace on the line after the location path throws")
{
    string conf = "server {\n"
                  "    host 127.0.0.1\n"
                  "    listen 8080\n"
                  "    root " + rootPath + "\n"
                  "    location /foo\n"
                  "    {\n"
                  "        allow GET\n"
                  "    }\n"
                  "}\n";

    Webserver server;
    CHECK_THROWS_AS(server.brackets(conf), WebservException);
}

// --- directive value edge cases ---

TEST_CASE_FIXTURE(ParserFixture, "listen with no argument defaults to port 80 rather than erroring")
{
    string conf = "server {\n"
                  "    host 127.0.0.1\n"
                  "    listen\n"
                  "    root " + rootPath + "\n"
                  "}\n";

    Webserver server;
    server.brackets(conf);
    parseGlobalDirectives(conf);
    server[0].parseServer(conf);

    CHECK_FALSE(configErrors.hasErrors());
    CHECK(server[0].getPort() == "80");
}

TEST_CASE_FIXTURE(ParserFixture, "root with no argument is a config error")
{
    string conf = "server {\n"
                  "    host 127.0.0.1\n"
                  "    listen 8080\n"
                  "    root\n"
                  "}\n";

    Webserver server;
    server.brackets(conf);
    parseGlobalDirectives(conf);
    server[0].parseServer(conf);

    CHECK(configErrors.hasErrors());
}

TEST_CASE_FIXTURE(ParserFixture, "a quoted argument with a space is not supported yet" * doctest::may_fail(true))
{
    // The planned Phase 3 lexer explicitly reads a quoted string as
    // one token; the current line-tokenizer (plain istream >>) has no
    // concept of quoting at all, so "/a path/" splits into two tokens
    // at the space - this locks in that gap, not a design choice.
    std::filesystem::path spaced = tmpDir / "a path";
    std::filesystem::create_directories(spaced);
    string conf = "server {\n"
                  "    host 127.0.0.1\n"
                  "    listen 8080\n"
                  "    root \"" + spaced.string() + "\"\n"
                  "}\n";

    Webserver server;
    server.brackets(conf);
    parseGlobalDirectives(conf);
    server[0].parseServer(conf);

    CHECK_FALSE(configErrors.hasErrors());
    CHECK(server[0].getRoot() == spaced.string());
}

TEST_CASE_FIXTURE(ParserFixture, "error_page is repeatable and never flagged as a duplicate")
{
    string conf = "server {\n"
                  "    host 127.0.0.1\n"
                  "    listen 8080\n"
                  "    root " + rootPath + "\n"
                  "    error_page 404 /404.html\n"
                  "    error_page 403 /403.html\n"
                  "    error_page 500 /500.html\n"
                  "}\n";

    Webserver server;
    server.brackets(conf);
    parseGlobalDirectives(conf);
    server[0].parseServer(conf);

    CHECK_FALSE(configErrors.hasErrors());
    CHECK(server[0].getErrorPages().size() == 3);
}

TEST_CASE_FIXTURE(ParserFixture, "cgi_path is repeatable and never flagged as a duplicate")
{
    string conf = "server {\n"
                  "    host 127.0.0.1\n"
                  "    listen 8080\n"
                  "    root " + rootPath + "\n"
                  "\n"
                  "    location / {\n"
                  "        root " + rootPath + "\n"
                  "        cgi_path py /usr/bin/python3\n"
                  "        cgi_path php /usr/bin/php-cgi\n"
                  "    }\n"
                  "}\n";

    Webserver server;
    server.brackets(conf);
    parseGlobalDirectives(conf);
    server[0].parseServer(conf);

    CHECK_FALSE(configErrors.hasErrors());
    CHECK(server[0].getLocations().at("/")->getCgiPaths().size() == 2);
}

// --- comments ---

TEST_CASE_FIXTURE(ParserFixture, "a comment on its own line is ignored")
{
    string conf = "server {\n"
                  "    # this is a comment\n"
                  "    host 127.0.0.1\n"
                  "    listen 8080\n"
                  "    root " + rootPath + "\n"
                  "}\n";

    Webserver server;
    server.brackets(conf);
    parseGlobalDirectives(conf);
    server[0].parseServer(conf);

    CHECK_FALSE(configErrors.hasErrors());
    CHECK(server[0].getPort() == "8080");
}

TEST_CASE_FIXTURE(ParserFixture, "a comment appended after a genuinely single-token directive is harmless")
{
    // root only ever reads one token for its path, so trailing text on
    // the same line - comment or not - is simply never consumed by it
    // (it's just abandoned when the line's stringstream is discarded
    // for the next getline() call).
    string conf = "server {\n"
                  "    host 127.0.0.1\n"
                  "    listen 8080\n"
                  "    root " + rootPath + " # the doc root\n"
                  "}\n";

    Webserver server;
    server.brackets(conf);
    parseGlobalDirectives(conf);
    server[0].parseServer(conf);

    CHECK_FALSE(configErrors.hasErrors());
    CHECK(server[0].getRoot() == rootPath);
}

TEST_CASE_FIXTURE(ParserFixture, "a comment appended after listen is read as an invalid ssl option" * doctest::may_fail(true))
{
    // listen isn't actually single-token: after the port, it
    // optionally reads one more token expecting exactly "ssl". A
    // trailing "# comment" gets read as that second token and
    // rejected as an invalid listen option - a real, slightly
    // surprising gap distinct from the genuinely-single-token
    // directives above, worth its own case rather than being folded
    // into "comments are harmless."
    string conf = "server {\n"
                  "    host 127.0.0.1\n"
                  "    listen 8080 # the main port\n"
                  "    root " + rootPath + "\n"
                  "}\n";

    Webserver server;
    server.brackets(conf);
    parseGlobalDirectives(conf);
    server[0].parseServer(conf);

    CHECK_FALSE(configErrors.hasErrors());
    CHECK(server[0].getPort() == "8080");
}

TEST_CASE_FIXTURE(ParserFixture, "a comment appended after a variadic directive is read as a literal value" * doctest::may_fail(true))
{
    // server_name reads every remaining token on the line via
    // while(line >> tmp) - it has no idea "#production" isn't meant
    // to be a real server name. This test documents that as a real
    // gap: it currently fails (the comment marker ends up as a second
    // server name) rather than being stripped.
    string conf = "server {\n"
                  "    host 127.0.0.1\n"
                  "    listen 8080\n"
                  "    root " + rootPath + "\n"
                  "    server_name example.com # production\n"
                  "}\n";

    Webserver server;
    server.brackets(conf);
    parseGlobalDirectives(conf);
    server[0].parseServer(conf);

    CHECK_FALSE(configErrors.hasErrors());
    CHECK(server[0].getServerNames().size() == 1);
}

// --- whitespace ---

TEST_CASE_FIXTURE(ParserFixture, "tabs between a directive and its value work the same as spaces")
{
    string conf = "server {\n"
                  "    host\t127.0.0.1\n"
                  "    listen\t8080\n"
                  "    root\t" + rootPath + "\n"
                  "}\n";

    Webserver server;
    server.brackets(conf);
    parseGlobalDirectives(conf);
    server[0].parseServer(conf);

    CHECK_FALSE(configErrors.hasErrors());
    CHECK(server[0].getHost() == "127.0.0.1");
    CHECK(server[0].getPort() == "8080");
}

TEST_CASE_FIXTURE(ParserFixture, "trailing whitespace on a directive line is trimmed")
{
    string conf = "server {   \n"
                  "    host 127.0.0.1   \n"
                  "    listen 8080\t\t\n"
                  "    root " + rootPath + "   \n"
                  "}\n";

    Webserver server;
    server.brackets(conf);
    parseGlobalDirectives(conf);
    server[0].parseServer(conf);

    CHECK_FALSE(configErrors.hasErrors());
    CHECK(server[0].getPort() == "8080");
}

TEST_CASE_FIXTURE(ParserFixture, "CRLF line endings do not break structural parsing" * doctest::may_fail(true))
{
    // getline() splits on '\n' only, leaving a trailing '\r' on every
    // line; trim() only strips " \t", not '\r'. Confirmed by running
    // this: it's worse than a corrupted value - brackets() requires
    // the "{" on a "server {" line to be the exact last thing on that
    // line (checked via line.get() == EOF right after extracting it),
    // and '\r' counts as stream-whitespace to >> but is still a real
    // character sitting after it, so line.get() returns '\r' instead
    // of EOF and the whole thing throws "Invalid brackets" before
    // parseServer() ever runs. Real gap, not a design choice.
    string conf = "server {\r\n"
                  "    host 127.0.0.1\r\n"
                  "    listen 8080\r\n"
                  "    root " + rootPath + "\r\n"
                  "}\r\n";

    Webserver server;
    server.brackets(conf);
    parseGlobalDirectives(conf);
    server[0].parseServer(conf);

    CHECK_FALSE(configErrors.hasErrors());
    CHECK(server[0].getPort() == "8080");
}

// --- main-context directive order independence ---

TEST_CASE_FIXTURE(ParserFixture, "error_log between two server blocks parses with zero errors")
{
    string conf = "server {\n"
                  "    host 127.0.0.1\n"
                  "    listen 8080\n"
                  "    root " + rootPath + "\n"
                  "}\n"
                  "\n"
                  "error_log /tmp/webserv-edge-case-test.log warn\n"
                  "\n"
                  "server {\n"
                  "    host 127.0.0.1\n"
                  "    listen 8081\n"
                  "    root " + rootPath + "\n"
                  "}\n";

    Webserver server;
    server.brackets(conf);
    parseGlobalDirectives(conf);
    REQUIRE(server._servers.size() == 2);
    server[0].parseServer(conf);
    server[1].parseServer(conf);

    CHECK_FALSE(configErrors.hasErrors());
    CHECK(errorLogPath == "/tmp/webserv-edge-case-test.log");
    CHECK(errorLogLevel == "warn");
}

TEST_CASE_FIXTURE(ParserFixture, "the user directive does not exist yet on this branch" * doctest::may_fail(true))
{
    // Ported from v5's Phase 1 (still-unmerged, see V4-PLAN.md
    // "Execution order"): a real account with no group is valid there.
    // On this branch it isn't recognized at all yet - Phase 4 below is
    // what actually carries it into the new parser, at which point
    // this stops being a may_fail case.
    string conf = "user root\n"
                  "\n"
                  "server {\n"
                  "    host 127.0.0.1\n"
                  "    listen 8080\n"
                  "    root " + rootPath + "\n"
                  "}\n";

    Webserver server;
    server.brackets(conf);
    parseGlobalDirectives(conf);
    server[0].parseServer(conf);

    CHECK_FALSE(configErrors.hasErrors());
}
