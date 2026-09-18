// Lexer unit tests. Tested directly against the token stream it
// produces, independent of anything about server/location semantics
// - a token stream for a given input is either exactly right or it
// isn't. No fixture needed: Lexer is self-contained per instance, no
// shared/global state to reset.

#include <doctest/doctest.h>
#include "../../inc/Lexer.hpp"

static void checkWord(Token const &t, string const &value)
{
    CHECK(t.type == TokenType::WORD);
    CHECK(t.value == value);
}

TEST_CASE("empty input produces a single END token")
{
    Lexer lexer("");
    vector<Token> tokens = lexer.tokenize();

    REQUIRE(tokens.size() == 1);
    CHECK(tokens[0].type == TokenType::END);
}

TEST_CASE("whitespace-only input produces a single END token")
{
    Lexer lexer("   \t\n  \r\n   ");
    vector<Token> tokens = lexer.tokenize();

    REQUIRE(tokens.size() == 1);
    CHECK(tokens[0].type == TokenType::END);
}

TEST_CASE("a single bare word")
{
    Lexer lexer("hello");
    vector<Token> tokens = lexer.tokenize();

    REQUIRE(tokens.size() == 2);
    checkWord(tokens[0], "hello");
    CHECK(tokens[1].type == TokenType::END);
}

TEST_CASE("multiple words separated by spaces and tabs")
{
    Lexer lexer("host 127.0.0.1\tlisten  8080");
    vector<Token> tokens = lexer.tokenize();

    REQUIRE(tokens.size() == 5);
    checkWord(tokens[0], "host");
    checkWord(tokens[1], "127.0.0.1");
    checkWord(tokens[2], "listen");
    checkWord(tokens[3], "8080");
    CHECK(tokens[4].type == TokenType::END);
}

TEST_CASE("braces are their own tokens regardless of surrounding whitespace")
{
    // "server{" and "server {" and "server\n{" all have to lex
    // identically - that's what makes brace placement stop mattering
    // to the parser built on top of this.
    Lexer sameLine("server {");
    Lexer noSpace("server{");
    Lexer nextLine("server\n{");

    for (Lexer *lexer : {&sameLine, &noSpace, &nextLine})
    {
        vector<Token> tokens = lexer->tokenize();
        REQUIRE(tokens.size() == 3);
        checkWord(tokens[0], "server");
        CHECK(tokens[1].type == TokenType::BRACE_OPEN);
        CHECK(tokens[2].type == TokenType::END);
    }
}

TEST_CASE("nested braces tokenize as a flat sequence")
{
    Lexer lexer("server { location / { allow GET } }");
    vector<Token> tokens = lexer.tokenize();

    REQUIRE(tokens.size() == 10);
    checkWord(tokens[0], "server");
    CHECK(tokens[1].type == TokenType::BRACE_OPEN);
    checkWord(tokens[2], "location");
    checkWord(tokens[3], "/");
    CHECK(tokens[4].type == TokenType::BRACE_OPEN);
    checkWord(tokens[5], "allow");
    checkWord(tokens[6], "GET");
    CHECK(tokens[7].type == TokenType::BRACE_CLOSE);
    CHECK(tokens[8].type == TokenType::BRACE_CLOSE);
    CHECK(tokens[9].type == TokenType::END);
}

TEST_CASE("a comment-only line produces no tokens for that line")
{
    Lexer lexer("# just a comment\nhost 127.0.0.1");
    vector<Token> tokens = lexer.tokenize();

    REQUIRE(tokens.size() == 3);
    checkWord(tokens[0], "host");
    checkWord(tokens[1], "127.0.0.1");
    CHECK(tokens[2].type == TokenType::END);
}

TEST_CASE("a comment appended after real content is stripped, not tokenized")
{
    // Fixes two real comment gaps at once: listen's trailing comment
    // no longer looks like a second directive argument, and a
    // variadic directive's trailing comment no longer looks like an
    // extra value - the lexer strips it before either directive ever
    // sees a token for it.
    Lexer lexer("listen 8080 # the main port\nserver_name example.com # prod");
    vector<Token> tokens = lexer.tokenize();

    REQUIRE(tokens.size() == 5);
    checkWord(tokens[0], "listen");
    checkWord(tokens[1], "8080");
    checkWord(tokens[2], "server_name");
    checkWord(tokens[3], "example.com");
    CHECK(tokens[4].type == TokenType::END);
}

TEST_CASE("a quoted string with a space is one word token")
{
    Lexer lexer("root \"/a path/with spaces\"");
    vector<Token> tokens = lexer.tokenize();

    REQUIRE(tokens.size() == 3);
    checkWord(tokens[0], "root");
    checkWord(tokens[1], "/a path/with spaces");
    CHECK(tokens[2].type == TokenType::END);
}

TEST_CASE("an empty quoted string is a word token with an empty value")
{
    Lexer lexer("server_name \"\"");
    vector<Token> tokens = lexer.tokenize();

    REQUIRE(tokens.size() == 3);
    checkWord(tokens[0], "server_name");
    checkWord(tokens[1], "");
    CHECK(tokens[2].type == TokenType::END);
}

TEST_CASE("an unterminated quoted string throws")
{
    Lexer lexer("root \"/never closed");
    CHECK_THROWS_AS(lexer.tokenize(), WebservException);
}

TEST_CASE("CRLF line endings do not corrupt adjacent tokens")
{
    // The direct regression test for a real gap found in the old
    // line-by-line parser: '\r' is just whitespace here, stripped
    // the same as a space or '\n' - it can never end up glued onto
    // the end of a word or make a brace-placement check fail the way
    // it did before.
    Lexer lexer("server {\r\n    host 127.0.0.1\r\n    listen 8080\r\n}\r\n");
    vector<Token> tokens = lexer.tokenize();

    REQUIRE(tokens.size() == 8);
    checkWord(tokens[0], "server");
    CHECK(tokens[1].type == TokenType::BRACE_OPEN);
    checkWord(tokens[2], "host");
    checkWord(tokens[3], "127.0.0.1");
    checkWord(tokens[4], "listen");
    checkWord(tokens[5], "8080");
    CHECK(tokens[6].type == TokenType::BRACE_CLOSE);
    CHECK(tokens[7].type == TokenType::END);
}

TEST_CASE("line numbers track newlines correctly")
{
    Lexer lexer("host 127.0.0.1\nlisten 8080\n\nroot /tmp\n");
    vector<Token> tokens = lexer.tokenize();

    REQUIRE(tokens.size() == 7);
    CHECK(tokens[0].line == 1); // host
    CHECK(tokens[1].line == 1); // 127.0.0.1
    CHECK(tokens[2].line == 2); // listen
    CHECK(tokens[3].line == 2); // 8080
    CHECK(tokens[4].line == 4); // root
    CHECK(tokens[5].line == 4); // /tmp
}

TEST_CASE("a realistic multi-line server block tokenizes correctly end to end")
{
    string conf = "server {\n"
                  "    host 127.0.0.1\n"
                  "    listen 8080\n"
                  "    root /var/www\n"
                  "\n"
                  "    location /uploads {\n"
                  "        allow GET POST\n"
                  "        upload on\n"
                  "    }\n"
                  "}\n";
    Lexer lexer(conf);
    vector<Token> tokens = lexer.tokenize();

    vector<string> expectedWords = {
        "server", "host", "127.0.0.1", "listen", "8080", "root", "/var/www",
        "location", "/uploads", "allow", "GET", "POST", "upload", "on",
    };
    size_t wordIndex = 0;
    for (Token const &t : tokens)
    {
        if (t.type == TokenType::WORD)
        {
            REQUIRE(wordIndex < expectedWords.size());
            CHECK(t.value == expectedWords[wordIndex]);
            wordIndex++;
        }
    }
    CHECK(wordIndex == expectedWords.size());
    CHECK(tokens.back().type == TokenType::END);
}
