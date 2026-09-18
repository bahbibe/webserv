#pragma once
#include "webserv.hpp"
#include "Lexer.hpp"

class Webserver;
class Server;

// Builds on the Lexer's token stream to replace the four hand-rolled
// line-scanners this project used to have - Webserver::brackets(),
// the old free function parseGlobalDirectives(), Server::
// parseServer(), Server::parseLocation() - with one real
// recursive-descent parser: one place that always knows exactly what
// nesting level it's at, instead of four independently-reconstructed
// notions of the same thing, coordinated only by a static cross-call
// position hack.
//
// Owns grammar and block structure only. The actual per-directive
// validation stays exactly where it always lived - on Server
// (applyServerDirective()) and Location (applyLocationDirective()) -
// called from here once a directive's name and same-line value
// tokens are known; this class never duplicates that logic.
class ConfigParser
{
public:
    explicit ConfigParser(string const &source);
    // Populates webserver._servers (one Server per "server {}" block)
    // and the main-context globals (pid/error_log/user) - what
    // main.cpp used to do with four separate calls, in one pass over
    // one token stream.
    void parse(Webserver &webserver);

private:
    vector<Token> _tokens;
    size_t _pos;

    Token const &peek() const;
    Token const &advance();
    bool checkWord(string const &value) const;
    bool checkBraceClose() const;
    void expectBraceOpen();
    void expectBraceClose();

    // Collects every WORD token sharing directiveLine's source line,
    // stopping at a brace or a line change - the same "a directive's
    // arguments are everything else on its own physical line"
    // semantics the old getline()-based parser enforced structurally,
    // now driven by token line numbers instead of splitting on '\n'.
    // A quoted multi-word value the Lexer already read as one token
    // stays one value here too - unlike rejoining tokens back into a
    // single string and re-splitting, this can't un-do that.
    vector<string> collectSameLineValues(int directiveLine);

    void parseServerBlock(Webserver &webserver);
    void parseLocationBlock(Server &server);
    void parseMainDirective(string const &name, vector<string> const &values);
};
