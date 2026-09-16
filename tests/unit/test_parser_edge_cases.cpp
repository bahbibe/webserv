// Edge-case tests for the config parser (see V4-PLAN.md Phase 2).
// Originally written against the old line-tokenizer, several
// deliberately locking in real gaps via doctest::may_fail(true).
// Phase 4 replaced the four hand-rolled scanners with a real
// Lexer + recursive-descent ConfigParser; its explicit acceptance bar
// is every one of those gaps closing for real, so all five may_fail
// markers are gone below - each of those cases now asserts the
// correct (not the old broken) behavior and genuinely passes.
//
// Two structural tests also changed in a way worth flagging: "brace
// on the line after the keyword" used to throw, because the old
// per-line scanner required "server {" and "location <path> {" to be
// on one physical line. The new Lexer emits braces as their own
// tokens independent of line breaks (see V4-PLAN.md Phase 3 and the
// "same source line" strategy note in ConfigParser), so this is no
// longer a structural error - it's just valid config now, same as
// nginx itself allows. Both tests were flipped to assert successful
// parsing instead of a throw.

#include <doctest/doctest.h>
#include "ParserFixture.hpp"
#include "../../inc/ConfigParser.hpp"

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
    CHECK_THROWS_AS(ConfigParser(conf).parse(server), WebservException);
}

TEST_CASE_FIXTURE(ParserFixture, "server brace on the line after the server keyword now parses fine")
{
    string conf = "server\n"
                  "{\n"
                  "    host 127.0.0.1\n"
                  "    listen 8080\n"
                  "    root " + rootPath + "\n"
                  "}\n";

    Webserver server;
    ConfigParser(conf).parse(server);

    REQUIRE(server._servers.size() == 1);
    CHECK_FALSE(configErrors.hasErrors());
    CHECK(server[0].getPort() == "8080");
}

TEST_CASE_FIXTURE(ParserFixture, "location brace on the line after the location path now parses fine")
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
    ConfigParser(conf).parse(server);

    CHECK_FALSE(configErrors.hasErrors());
    auto const &locations = server[0].getLocations();
    REQUIRE(locations.find("/foo") != locations.end());
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
    ConfigParser(conf).parse(server);

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
    ConfigParser(conf).parse(server);

    CHECK(configErrors.hasErrors());
}

TEST_CASE_FIXTURE(ParserFixture, "a quoted argument with a space is supported")
{
    // The Lexer (Phase 3) reads a quoted string as one token, so
    // "/a path/" no longer splits into two tokens at the space -
    // this closed what used to be a real, locked-in gap.
    std::filesystem::path spaced = tmpDir / "a path";
    std::filesystem::create_directories(spaced);
    string conf = "server {\n"
                  "    host 127.0.0.1\n"
                  "    listen 8080\n"
                  "    root \"" + spaced.string() + "\"\n"
                  "}\n";

    Webserver server;
    ConfigParser(conf).parse(server);

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
    ConfigParser(conf).parse(server);

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
    ConfigParser(conf).parse(server);

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
    ConfigParser(conf).parse(server);

    CHECK_FALSE(configErrors.hasErrors());
    CHECK(server[0].getPort() == "8080");
}

TEST_CASE_FIXTURE(ParserFixture, "a comment appended after a genuinely single-token directive is harmless")
{
    string conf = "server {\n"
                  "    host 127.0.0.1\n"
                  "    listen 8080\n"
                  "    root " + rootPath + " # the doc root\n"
                  "}\n";

    Webserver server;
    ConfigParser(conf).parse(server);

    CHECK_FALSE(configErrors.hasErrors());
    CHECK(server[0].getRoot() == rootPath);
}

TEST_CASE_FIXTURE(ParserFixture, "a comment appended after listen is stripped at lex time, not read as an ssl option")
{
    // The Lexer strips comments before the parser ever sees a token
    // stream, so a trailing "# comment" after listen's port no longer
    // reaches the "expects exactly ssl" check at all.
    string conf = "server {\n"
                  "    host 127.0.0.1\n"
                  "    listen 8080 # the main port\n"
                  "    root " + rootPath + "\n"
                  "}\n";

    Webserver server;
    ConfigParser(conf).parse(server);

    CHECK_FALSE(configErrors.hasErrors());
    CHECK(server[0].getPort() == "8080");
}

TEST_CASE_FIXTURE(ParserFixture, "a comment appended after a variadic directive is stripped, not read as a literal value")
{
    string conf = "server {\n"
                  "    host 127.0.0.1\n"
                  "    listen 8080\n"
                  "    root " + rootPath + "\n"
                  "    server_name example.com # production\n"
                  "}\n";

    Webserver server;
    ConfigParser(conf).parse(server);

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
    ConfigParser(conf).parse(server);

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
    ConfigParser(conf).parse(server);

    CHECK_FALSE(configErrors.hasErrors());
    CHECK(server[0].getPort() == "8080");
}

TEST_CASE_FIXTURE(ParserFixture, "CRLF line endings do not break structural parsing")
{
    // The Lexer treats '\r' as ordinary whitespace right alongside
    // space/tab/'\n', so a leftover '\r' from a Windows-style line
    // ending is just a separator now, not a stray character that
    // breaks the old strict "brace must be the last thing on the
    // line" check.
    string conf = "server {\r\n"
                  "    host 127.0.0.1\r\n"
                  "    listen 8080\r\n"
                  "    root " + rootPath + "\r\n"
                  "}\r\n";

    Webserver server;
    ConfigParser(conf).parse(server);

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
    ConfigParser(conf).parse(server);

    REQUIRE(server._servers.size() == 2);
    CHECK_FALSE(configErrors.hasErrors());
    CHECK(errorLogPath == "/tmp/webserv-edge-case-test.log");
    CHECK(errorLogLevel == "warn");
}

TEST_CASE_FIXTURE(ParserFixture, "the user directive is recognized and validated")
{
    // Ported in as part of Phase 4's main-context directive handling
    // (see V4-PLAN.md); the actual privilege-drop mechanism itself is
    // v5's remaining work, but the directive is parsed and validated
    // here already.
    string conf = "user root\n"
                  "\n"
                  "server {\n"
                  "    host 127.0.0.1\n"
                  "    listen 8080\n"
                  "    root " + rootPath + "\n"
                  "}\n";

    Webserver server;
    ConfigParser(conf).parse(server);

    CHECK_FALSE(configErrors.hasErrors());
    CHECK(dropUser == "root");
}
