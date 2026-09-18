#pragma once
#include "../../inc/Server.hpp"
#include <filesystem>
#include <fstream>

// Shared setup/teardown for every parser unit test. Two jobs:
//
// 1. Resets the process-global state parsing touches - configErrors,
//    the pid/error_log/user globals, confDir - so each test case
//    starts from a clean slate regardless of what ran before it in
//    the same test binary. Production code never needs this (one
//    config is parsed once per process lifetime); doctest runs every
//    TEST_CASE in the same process, one after another.
// 2. Provides real, existing filesystem fixtures - a root directory, a
//    dummy TLS cert/key pair - since the parser's own validation
//    (root/ssl_certificate/ssl_certificate_key all check the path
//    actually exists via access()) needs something real to point at,
//    not just a plausible-looking string.
struct ParserFixture
{
    std::filesystem::path tmpDir;
    std::string rootPath;
    std::string certPath;
    std::string keyPath;

    ParserFixture()
    {
        configErrors.clear();
        pidPath.clear();
        errorLogPath.clear();
        errorLogLevel = "info";
        dropUser.clear();
        dropGroup.clear();
        confDir = "conf/";
        // Webserver's constructor opens accessLogPath and warns if it
        // can't - harmless for parser tests (nothing here ever writes
        // through it), but pointing it at /dev/null keeps 14+ warning
        // lines out of the test output for no real signal.
        accessLogPath = "/dev/null";

        tmpDir = std::filesystem::temp_directory_path() / "webserv_parser_fixture";
        std::filesystem::remove_all(tmpDir);
        std::filesystem::create_directories(tmpDir);
        rootPath = tmpDir.string();
        certPath = (tmpDir / "cert.pem").string();
        keyPath = (tmpDir / "key.pem").string();
        std::ofstream(certPath) << "dummy cert - existence is all the parser checks\n";
        std::ofstream(keyPath) << "dummy key - existence is all the parser checks\n";
    }

    ~ParserFixture()
    {
        std::filesystem::remove_all(tmpDir);
    }
};
