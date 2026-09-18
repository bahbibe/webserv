#pragma once
#include "webserv.hpp"

enum class TokenType
{
    WORD,
    BRACE_OPEN,
    BRACE_CLOSE,
    END
};

struct Token
{
    TokenType type;
    string value;   // only meaningful for WORD
    int line;       // 1-based, for error messages
};

// Turns raw config file text into one flat token stream - comments
// stripped, every whitespace character (including '\r', unlike the
// old line-by-line parser) treated uniformly as a separator, a
// quoted string read as one token regardless of any spaces inside
// it. One place decides what a token is, instead of four
// line-scanners each doing their own ad hoc splitting. Purely
// lexical - has no idea what a "server" or "location" is; that's
// ConfigParser's job, built on top of the token stream this
// produces.
class Lexer
{
public:
    explicit Lexer(string const &source);
    vector<Token> tokenize();

private:
    // By value, not by reference: a Lexer must never outlive the
    // string it was built from, and a reference member makes that a
    // silent lifetime trap (e.g. Lexer(someTemporaryString()) would
    // leave _source dangling the instant the constructor returns) -
    // a config file is at most a few KB, so one copy here costs
    // nothing worth trading real safety for.
    string _source;
    size_t _pos;
    int _line;

    bool atEnd() const;
    char peek() const;
    char advance();
    void skipWhitespaceAndComments();
    Token readBareWord();
    Token readQuotedWord();
};
