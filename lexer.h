#pragma once
#include <string>

// the following enum contains all possible token types
enum token_type{
    TOK_KEYWORD,
    TOK_IDENTIFIER,
    TOK_INT_LITERAL,
    TOK_CHAR_LITERAL,

    TOK_ASSIGN,   // =
    TOK_EQ,       // ==
    TOK_NE,       // !=
    TOK_LT,       // <
    TOK_LE,       // <=
    TOK_GT,       // >
    TOK_GE,       // >=
    TOK_AND,      // &&
    TOK_OR,       // ||
    TOK_NOT,      // !

    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_DIVIDE,

    TOK_LPAREN, TOK_RPAREN, TOK_LBRACE, TOK_RBRACE,
    TOK_COMMA, TOK_SEMI,

    TOK_WHITESPACE,  // this is skipped over n discarded
    TOK_COMMENT,     // also skipped over n discarded

    TOK_EOF,
    TOK_ERROR
};

// this struct stores information about a token
struct token {
    token_type type;  // the type of the token
    std::string lexeme;  // the string representation of the token
    int line;  // the line number where the token was found
    int column;  // the column number where the token was found
};

// this function builds the actual transition table for the
// finite state machine, it must be called once before the first next_token()
void build_transition_table();

// this function returns the string representation of a
// token type, mainly for output purposes
std::string get_token_name(token_type token);

// this function returns the name the grammar uses for a token
// (for example identifiers are "id" and end of file is "$")
std::string get_terminal_name(const token &t);

// this class manages everything related to the lexer
// it reads the input stream, and produces tokens
class lexer{
    private:
        std::string buffer;
        int pos;
        int line;
        int col;

        void advance(int from, int to);
    public:
        lexer(const std::string &source);

        token next_token();
};
