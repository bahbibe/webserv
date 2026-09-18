#include "../../inc/ConfigParser.hpp"
#include "../../inc/Server.hpp"

ConfigParser::ConfigParser(string const &source) : _pos(0)
{
    Lexer lexer(source);
    _tokens = lexer.tokenize();
}

Token const &ConfigParser::peek() const
{
    return _tokens[_pos];
}

Token const &ConfigParser::advance()
{
    Token const &t = _tokens[_pos];
    if (_pos + 1 < _tokens.size())
        _pos++;
    return t;
}

bool ConfigParser::checkWord(string const &value) const
{
    return peek().type == TokenType::WORD && peek().value == value;
}

bool ConfigParser::checkBraceClose() const
{
    return peek().type == TokenType::BRACE_CLOSE;
}

void ConfigParser::expectBraceOpen()
{
    if (peek().type != TokenType::BRACE_OPEN)
        throw WebservException(ERR "Invalid brackets");
    advance();
}

void ConfigParser::expectBraceClose()
{
    if (peek().type != TokenType::BRACE_CLOSE)
        throw WebservException(ERR "Invalid brackets");
    advance();
}

vector<string> ConfigParser::collectSameLineValues(int directiveLine)
{
    vector<string> values;
    while (peek().type == TokenType::WORD && peek().line == directiveLine)
        values.push_back(advance().value);
    return values;
}

void ConfigParser::parse(Webserver &webserver)
{
    while (peek().type != TokenType::END)
    {
        if (checkWord("server"))
        {
            parseServerBlock(webserver);
        }
        else if (peek().type == TokenType::WORD)
        {
            Token nameToken = advance();
            vector<string> values = collectSameLineValues(nameToken.line);
            parseMainDirective(nameToken.value, values);
        }
        else
        {
            throw WebservException(ERR "Invalid brackets");
        }
    }
}

void ConfigParser::parseServerBlock(Webserver &webserver)
{
    advance(); // "server"
    expectBraceOpen();

    webserver._servers.push_back(Server());
    Server &server = webserver._servers.back();
    server.mimeTypes();

    t_dir dir;
    memset(&dir, 0, sizeof(dir));

    while (!checkBraceClose() && peek().type != TokenType::END)
    {
        if (checkWord("location"))
        {
            parseLocationBlock(server);
            continue;
        }
        if (peek().type != TokenType::WORD)
            throw WebservException(ERR "Invalid brackets");
        Token nameToken = advance();
        vector<string> values = collectSameLineValues(nameToken.line);
        if (isServerDir(nameToken.value))
            server.applyServerDirective(nameToken.value, values, dir);
        else
            configErrors.add(ERR "Invalid directive at server level: " + nameToken.value);
    }
    expectBraceClose();
    server.finalizeServerDirectives(dir);
}

void ConfigParser::parseLocationBlock(Server &server)
{
    advance(); // "location"
    if (peek().type != TokenType::WORD)
        throw WebservException(ERR "Invalid brackets");
    string path = advance().value;
    expectBraceOpen();

    unique_ptr<Location> location = make_unique<Location>();
    while (!checkBraceClose() && peek().type != TokenType::END)
    {
        if (peek().type != TokenType::WORD)
            throw WebservException(ERR "Invalid brackets");
        Token nameToken = advance();
        vector<string> values = collectSameLineValues(nameToken.line);
        if (isLocationDir(nameToken.value))
            location->applyLocationDirective(nameToken.value, values);
        else
            configErrors.add(ERR "Invalid directive in location block: " + nameToken.value);
    }
    expectBraceClose();
    location->finalizeLocationDirectives(server.getRoot(), server.getAutoindex(), server.getClientMaxBodySize());
    server.addLocation(path, move(location));
}

// user <name> [group]; validated here via getpwnam()/getgrnam() at
// config-parse time, same as every other directive that needs a real
// OS lookup to validate (resolveHostFamily() for host, access() for
// root/ssl paths).
void ConfigParser::parseMainDirective(string const &name, vector<string> const &values)
{
    size_t idx = 0;
    if (name == "pid")
    {
        pidPath = idx < values.size() ? values[idx++] : "";
        if (pidPath.empty())
            configErrors.add(ERR "Invalid pid directive (needs a path)");
    }
    else if (name == "error_log")
    {
        errorLogPath = idx < values.size() ? values[idx++] : "";
        if (idx < values.size())
        {
            string level = values[idx++];
            static const string validLevels[] = {"trace", "debug", "info", "warn", "error", "critical", "off"};
            bool found = false;
            for (size_t i = 0; i < sizeof(validLevels) / sizeof(validLevels[0]); i++)
                if (level == validLevels[i])
                    found = true;
            if (!found)
                configErrors.add(ERR "Invalid error_log level: " + level);
            else
                errorLogLevel = level;
        }
        if (errorLogPath.empty())
            configErrors.add(ERR "Invalid error_log directive (needs a path)");
    }
    else if (name == "user")
    {
        dropUser = idx < values.size() ? values[idx++] : "";
        if (dropUser.empty())
        {
            configErrors.add(ERR "Invalid user directive (needs a username)");
            return;
        }
        if (!getpwnam(dropUser.c_str()))
            configErrors.add(ERR "user: no such account: " + dropUser);
        if (idx < values.size())
        {
            dropGroup = values[idx++];
            if (!getgrnam(dropGroup.c_str()))
                configErrors.add(ERR "user: no such group: " + dropGroup);
        }
    }
    else
        configErrors.add(ERR "Invalid directive at top level: " + name);
}
