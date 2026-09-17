#include "../../inc/Lexer.hpp"

Lexer::Lexer(string const &source) : _source(source), _pos(0), _line(1) {}

bool Lexer::atEnd() const
{
    return _pos >= _source.size();
}

char Lexer::peek() const
{
    return atEnd() ? '\0' : _source[_pos];
}

char Lexer::advance()
{
    char c = _source[_pos++];
    if (c == '\n')
        _line++;
    return c;
}

void Lexer::skipWhitespaceAndComments()
{
    while (!atEnd())
    {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
        {
            advance();
            continue;
        }
        if (c == '#')
        {
            while (!atEnd() && peek() != '\n')
                advance();
            continue;
        }
        break;
    }
}

// A bare (unquoted) word ends at the next whitespace, comment, or
// brace - including a brace with no whitespace before it ("server{"
// lexes identically to "server {" or "server\n{"), which is exactly
// what makes brace placement stop mattering to the parser built on
// top of this.
Token Lexer::readBareWord()
{
    int startLine = _line;
    string value;
    while (!atEnd())
    {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '#' || c == '{' || c == '}')
            break;
        value += advance();
    }
    return Token{TokenType::WORD, value, startLine};
}

Token Lexer::readQuotedWord()
{
    int startLine = _line;
    advance(); // opening quote
    string value;
    while (!atEnd() && peek() != '"')
        value += advance();
    if (atEnd())
        throw WebservException(ERR "Unterminated quoted string in config file");
    advance(); // closing quote
    return Token{TokenType::WORD, value, startLine};
}

vector<Token> Lexer::tokenize()
{
    vector<Token> tokens;
    while (true)
    {
        skipWhitespaceAndComments();
        if (atEnd())
        {
            tokens.push_back(Token{TokenType::END, "", _line});
            break;
        }
        char c = peek();
        if (c == '{')
        {
            int line = _line;
            advance();
            tokens.push_back(Token{TokenType::BRACE_OPEN, "{", line});
        }
        else if (c == '}')
        {
            int line = _line;
            advance();
            tokens.push_back(Token{TokenType::BRACE_CLOSE, "}", line});
        }
        else if (c == '"')
            tokens.push_back(readQuotedWord());
        else
            tokens.push_back(readBareWord());
    }
    return tokens;
}
