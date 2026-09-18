// Characterization tests for the config parser. Originally written
// against the four hand-rolled line-scanners, now driving
// ConfigParser - same test intent, same expected outcomes, just
// calling the parser main.cpp actually uses:
//   Webserver server;
//   ConfigParser(buff).parse(server);

#include <doctest/doctest.h>
#include "ParserFixture.hpp"
#include "../../inc/ConfigParser.hpp"

TEST_CASE_FIXTURE(ParserFixture, "minimal single server block parses with zero errors")
{
    string conf = "server {\n"
                  "    host 127.0.0.1\n"
                  "    listen 8080\n"
                  "    root " + rootPath + "\n"
                  "    index index.html\n"
                  "}\n";

    Webserver server;
    ConfigParser(conf).parse(server);

    REQUIRE(server._servers.size() == 1);
    CHECK_FALSE(configErrors.hasErrors());
    CHECK(server[0].getHost() == "127.0.0.1");
    CHECK(server[0].getPort() == "8080");
    CHECK(server[0].getRoot() == rootPath);
}

TEST_CASE_FIXTURE(ParserFixture, "full server block with a location parses correctly")
{
    string conf = "server {\n"
                  "    host 127.0.0.1\n"
                  "    listen 8080\n"
                  "    root " + rootPath + "\n"
                  "    index index.html\n"
                  "    client_max_body_size 1000000\n"
                  "    autoindex off\n"
                  "\n"
                  "    location /uploads {\n"
                  "        root " + rootPath + "\n"
                  "        allow GET POST DELETE\n"
                  "        upload on\n"
                  "        upload_path " + rootPath + "\n"
                  "        cgi on\n"
                  "        autoindex on\n"
                  "    }\n"
                  "}\n";

    Webserver server;
    ConfigParser(conf).parse(server);

    REQUIRE(server._servers.size() == 1);
    CHECK_FALSE(configErrors.hasErrors());
    CHECK(server[0].getClientMaxBodySize() == 1000000);
    CHECK_FALSE(server[0].getAutoindex());

    auto const &locations = server[0].getLocations();
    REQUIRE(locations.find("/uploads") != locations.end());
    Location const &loc = *locations.at("/uploads");
    CHECK(loc.getUpload());
    CHECK(loc.getCgi());
    CHECK(loc.getAutoindex());
    vector<string> methods = loc.getMethods();
    CHECK(find(methods.begin(), methods.end(), "GET") != methods.end());
    CHECK(find(methods.begin(), methods.end(), "POST") != methods.end());
    CHECK(find(methods.begin(), methods.end(), "DELETE") != methods.end());
}

TEST_CASE_FIXTURE(ParserFixture, "location inherits server client_max_body_size when not set, overrides when it is")
{
    string conf = "server {\n"
                  "    host 127.0.0.1\n"
                  "    listen 8080\n"
                  "    root " + rootPath + "\n"
                  "    client_max_body_size 1000000\n"
                  "\n"
                  "    location /inherits {\n"
                  "        root " + rootPath + "\n"
                  "    }\n"
                  "\n"
                  "    location /overrides {\n"
                  "        root " + rootPath + "\n"
                  "        client_max_body_size 50\n"
                  "    }\n"
                  "}\n";

    Webserver server;
    ConfigParser(conf).parse(server);

    CHECK_FALSE(configErrors.hasErrors());
    auto const &locations = server[0].getLocations();
    CHECK(locations.at("/inherits")->getClientMaxBodySize() == 1000000);
    CHECK(locations.at("/overrides")->getClientMaxBodySize() == 50);
}

TEST_CASE_FIXTURE(ParserFixture, "two server blocks in one file both parse independently")
{
    string conf = "server {\n"
                  "    host 127.0.0.1\n"
                  "    listen 8080\n"
                  "    root " + rootPath + "\n"
                  "}\n"
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
    CHECK(server[0].getPort() == "8080");
    CHECK(server[1].getPort() == "8081");
}

TEST_CASE_FIXTURE(ParserFixture, "pid and error_log before a server block parse with zero errors")
{
    string conf = "pid /tmp/webserv-characterization-test.pid\n"
                  "error_log /tmp/webserv-characterization-test.log debug\n"
                  "\n"
                  "server {\n"
                  "    host 127.0.0.1\n"
                  "    listen 8080\n"
                  "    root " + rootPath + "\n"
                  "}\n";

    Webserver server;
    ConfigParser(conf).parse(server);

    CHECK_FALSE(configErrors.hasErrors());
    CHECK(pidPath == "/tmp/webserv-characterization-test.pid");
    CHECK(errorLogPath == "/tmp/webserv-characterization-test.log");
    CHECK(errorLogLevel == "debug");
}

TEST_CASE_FIXTURE(ParserFixture, "pid directive after a server block still parses with zero errors")
{
    // Regression test: the old Server::parseServer() had no concept
    // of "content that isn't mine to validate," so a main-context
    // directive appearing anywhere near a server block's own text was
    // liable to be misread. ConfigParser's grammar handles this
    // structurally - a main-context directive is just another
    // top-level production, wherever it appears.
    string conf = "server {\n"
                  "    host 127.0.0.1\n"
                  "    listen 8080\n"
                  "    root " + rootPath + "\n"
                  "}\n"
                  "\n"
                  "pid /tmp/webserv-characterization-test.pid\n";

    Webserver server;
    ConfigParser(conf).parse(server);

    CHECK_FALSE(configErrors.hasErrors());
    CHECK(pidPath == "/tmp/webserv-characterization-test.pid");
}

TEST_CASE_FIXTURE(ParserFixture, "an unrecognized top-level directive produces exactly one error")
{
    // Regression test for the exact v3 bug: a genuinely invalid
    // top-level directive used to be reported twice - once correctly
    // by parseGlobalDirectives(), once confusingly as "invalid at
    // server level" by parseServer(). ConfigParser's single-pass
    // grammar makes double-reporting structurally impossible: each
    // token is consumed by exactly one production.
    string conf = "worker_processes 4\n"
                  "\n"
                  "server {\n"
                  "    host 127.0.0.1\n"
                  "    listen 8080\n"
                  "    root " + rootPath + "\n"
                  "}\n";

    Webserver server;
    ConfigParser(conf).parse(server);

    REQUIRE(configErrors.hasErrors());
    CHECK(configErrors.errorCount() == 1);
}

TEST_CASE_FIXTURE(ParserFixture, "listen ... ssl with both cert files present parses with zero errors")
{
    string conf = "server {\n"
                  "    host 127.0.0.1\n"
                  "    listen 8443 ssl\n"
                  "    ssl_certificate " + certPath + "\n"
                  "    ssl_certificate_key " + keyPath + "\n"
                  "    root " + rootPath + "\n"
                  "}\n";

    Webserver server;
    ConfigParser(conf).parse(server);

    CHECK_FALSE(configErrors.hasErrors());
    CHECK(server[0].getSsl());
}

TEST_CASE_FIXTURE(ParserFixture, "listen ... ssl without ssl_certificate is exactly one error")
{
    string conf = "server {\n"
                  "    host 127.0.0.1\n"
                  "    listen 8443 ssl\n"
                  "    root " + rootPath + "\n"
                  "}\n";

    Webserver server;
    ConfigParser(conf).parse(server);

    REQUIRE(configErrors.hasErrors());
    CHECK(configErrors.errorCount() == 1);
}

TEST_CASE_FIXTURE(ParserFixture, "a duplicate server-level directive is a config error")
{
    string conf = "server {\n"
                  "    host 127.0.0.1\n"
                  "    listen 8080\n"
                  "    listen 8081\n"
                  "    root " + rootPath + "\n"
                  "}\n";

    Webserver server;
    ConfigParser(conf).parse(server);

    CHECK(configErrors.hasErrors());
}

TEST_CASE_FIXTURE(ParserFixture, "a server block missing its closing brace throws")
{
    string conf = "server {\n"
                  "    host 127.0.0.1\n"
                  "    listen 8080\n";

    Webserver server;
    CHECK_THROWS_AS(ConfigParser(conf).parse(server), WebservException);
}

TEST_CASE_FIXTURE(ParserFixture, "a stray closing brace with nothing open throws")
{
    string conf = "server {\n"
                  "    host 127.0.0.1\n"
                  "}\n"
                  "}\n";

    Webserver server;
    CHECK_THROWS_AS(ConfigParser(conf).parse(server), WebservException);
}

TEST_CASE_FIXTURE(ParserFixture, "an empty file produces zero servers and zero errors")
{
    string conf = "";

    Webserver server;
    ConfigParser(conf).parse(server);

    CHECK(server._servers.size() == 0);
    CHECK_FALSE(configErrors.hasErrors());
}

TEST_CASE_FIXTURE(ParserFixture, "a file that's only comments and whitespace produces zero servers and zero errors")
{
    string conf = "# just a comment\n"
                  "\n"
                  "   \n"
                  "# another comment\n";

    Webserver server;
    ConfigParser(conf).parse(server);

    CHECK(server._servers.size() == 0);
    CHECK_FALSE(configErrors.hasErrors());
}
