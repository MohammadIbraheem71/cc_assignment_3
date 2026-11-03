#include "lexer.h"

#include <iostream>
#include <initializer_list>
#include <unordered_set>

#include <cctype>

// this set stores keywords
const std::unordered_set<std::string> keywords = {
    "if", "else", "while", "return", "int", "char"
};

// the following enum contains all possible character types
// note that this is not the char object type in c++
// but rather to classify the input stream characters into
// different types for the lexer to process
enum char_type{
    C_LETTER, C_DIGIT, C_QUOTE,
    C_EQ, C_LT, C_GT, C_BANG, C_AMP, C_PIPE, C_SLASH,
    C_STAR, C_PLUS, C_MINUS,
    C_LPAREN, C_RPAREN, C_LBRACE, C_RBRACE, C_COMMA, C_SEMI,
    C_SPACE, C_NEWLINE,
    C_OTHER,          // anything not covered above -> always illegal at S_START
    NUM_CLASSES
};

// the following enum contains all possible states of the lexer
// finite state machine
enum state {
    S_START,

    S_ID,          // seen letter, may continue with letter|digit
    S_NUM,         // seen digit,  may continue with digit

    S_QUOTE1,      // seen opening quote, expect exactly char
    S_CHARVAL,     // seen the char inside a quote, expect closing '
    S_CHARDONE,    // seen closing quote, CHAR_LITERAL complete

    S_EQ1,         // seen '=' could become '=='
    S_EQEQ,        // seen '=='

    S_LT1,         // seen '<'  could become '<='
    S_LE,          // seen '<='

    S_GT1,         // seen '>' could become '>='
    S_GE,          // seen '>='

    S_NOT1,        // seen '!' could become '!='
    S_NE,          // seen '!='

    S_AND1,        // seen '&'MUST become '&&' (no bitwise &)
    S_ANDAND,      // seen '&&'

    S_OR1,         // seen '|' MUST become '||' (no bitwise | for this language)
    S_OROR,        // seen '||'

    S_SLASH1,      // seen '/' could become '//' comment
    S_COMMENT,     // inside a // comment, consuming until newline/EOF

    S_WS,          // consuming whitespace

    // these are all single character tokens,
    // one transition from S_START to these states will complete the token
    S_PLUS, S_MINUS, S_STAR,
    S_LPAREN, S_RPAREN, S_LBRACE, S_RBRACE, S_COMMA, S_SEMI,
    S_SLASHDONE,   // accepts a lone '/' as DIVIDE

    NUM_STATES
};

int table[NUM_STATES][NUM_CLASSES];  // the transition table for the lexer

bool is_accepting[NUM_STATES]; // whether a state is final (accepting) or not
token_type accepting_token[NUM_STATES]; // the token type for each accepting state

// this function adds a transition between 2 states
// in the transition table
void add_transition(state from, char_type input, state to){
    table[from][input] = to;
}

// this function adds transition to all classes except specified ones
void add_transition_except(state from, state to, std::initializer_list<char_type> excluded ){
    for (int c = 0; c < NUM_CLASSES; c++){
        bool skip = false;
        for (char_type ex : excluded){
            if (ex == c){
                skip = true;
            }
        }

        if (!skip){
            table[from][c] = to;
        }
    }
}

// this function marks a state as accepting and associates it with a token type
void mark_accepting(state s, token_type token){
    is_accepting[s] = true;
    accepting_token[s] = token;
}


// this function returns the string representation of a
// token type, mainly for output purposes
std::string get_token_name(token_type token){
    switch(token){
        case TOK_KEYWORD:      return "KEYWORD";
        case TOK_IDENTIFIER:   return "IDENTIFIER";
        case TOK_INT_LITERAL:  return "INT_LITERAL";
        case TOK_CHAR_LITERAL: return "CHAR_LITERAL";
        case TOK_ASSIGN:       return "ASSIGN";
        case TOK_EQ:            return "EQ";
        case TOK_NE:            return "NE";
        case TOK_LT:            return "LT";
        case TOK_LE:            return "LE";
        case TOK_GT:            return "GT";
        case TOK_GE:            return "GE";
        case TOK_AND:           return "AND";
        case TOK_OR:             return "OR";
        case TOK_NOT:            return "NOT";
        case TOK_PLUS:           return "PLUS";
        case TOK_MINUS:          return "MINUS";
        case TOK_STAR:           return "STAR";
        case TOK_DIVIDE:         return "DIVIDE";
        case TOK_LPAREN:         return "LPAREN";
        case TOK_RPAREN:         return "RPAREN";
        case TOK_LBRACE:         return "LBRACE";
        case TOK_RBRACE:         return "RBRACE";
        case TOK_COMMA:          return "COMMA";
        case TOK_SEMI:           return "SEMI";
        case TOK_EOF:            return "EOF";
        case TOK_ERROR:          return "ERROR";

        default:
            // if nothing matches, then i just return a "?"
            return "?";
    }
}

// this function returns the name the grammar uses for a token
// the parse table is indexed by these names
std::string get_terminal_name(const token &t){
    switch(t.type){
        // a keyword is its own terminal: int, char, if, else, while, return
        case TOK_KEYWORD:      return t.lexeme;
        case TOK_IDENTIFIER:   return "id";
        case TOK_INT_LITERAL:  return "num";
        case TOK_CHAR_LITERAL: return "char_lit";
        case TOK_ASSIGN:       return "assign";
        case TOK_EQ:           return "eq";
        case TOK_NE:           return "ne";
        case TOK_LT:           return "lt";
        case TOK_LE:           return "le";
        case TOK_GT:           return "gt";
        case TOK_GE:           return "ge";
        case TOK_AND:          return "and";
        case TOK_OR:           return "or";
        case TOK_NOT:          return "not";
        case TOK_PLUS:         return "plus";
        case TOK_MINUS:        return "minus";
        case TOK_STAR:         return "star";
        case TOK_DIVIDE:       return "slash";
        case TOK_LPAREN:       return "lparen";
        case TOK_RPAREN:       return "rparen";
        case TOK_LBRACE:       return "lbrace";
        case TOK_RBRACE:       return "rbrace";
        case TOK_COMMA:        return "comma";
        case TOK_SEMI:         return "semi";
        case TOK_EOF:          return "$";

        default:
            // nothing in the grammar matches this token
            return "?";
    }
}

// this function inputs a character
// and outputs the character type, as defined in the char_type enum
char_type classify_char(char c){
    if (isalpha((unsigned char)c) || c == '_'){
        return C_LETTER;
    }
    if (isdigit((unsigned char)c)){
        return C_DIGIT;
    }
    switch (c) {
        case '\'': return C_QUOTE;
        case '=':  return C_EQ;
        case '<':  return C_LT;
        case '>':  return C_GT;
        case '!':  return C_BANG;
        case '&':  return C_AMP;
        case '|':  return C_PIPE;
        case '/':  return C_SLASH;
        case '*':  return C_STAR;
        case '+':  return C_PLUS;
        case '-':  return C_MINUS;
        case '(':  return C_LPAREN;
        case ')':  return C_RPAREN;
        case '{':  return C_LBRACE;
        case '}':  return C_RBRACE;
        case ',':  return C_COMMA;
        case ';':  return C_SEMI;

        case ' ':   // for any of the following three, return C_SPACE
        case '\t':
        case '\r': return C_SPACE;

        case '\n': return C_NEWLINE;
        default:   return C_OTHER;
    }
}

// this function builds the actual transition table for the
// finite state machine
// any changes/modifications to the transition table should be done in this
// function
void build_transition_table(){

    // we start every transition as dead, and every state as non-accepting
    for (int s = 0; s < NUM_STATES; s++){
        is_accepting[s] = false;
        for (int c = 0; c < NUM_CLASSES; c++){
            table[s][c] = -1;  // transition doesnt lead to anywhere
        }
    }

    // from the start state, the first character decides which family of token
    // we r about to build
    add_transition(S_START, C_LETTER,  S_ID);
    add_transition(S_START, C_DIGIT,   S_NUM);
    add_transition(S_START, C_QUOTE,   S_QUOTE1);
    add_transition(S_START, C_EQ,      S_EQ1);
    add_transition(S_START, C_LT,      S_LT1);
    add_transition(S_START, C_GT,      S_GT1);
    add_transition(S_START, C_BANG,    S_NOT1);
    add_transition(S_START, C_AMP,     S_AND1);
    add_transition(S_START, C_PIPE,    S_OR1);
    add_transition(S_START, C_SLASH,   S_SLASH1);
    add_transition(S_START, C_STAR,    S_STAR);
    add_transition(S_START, C_PLUS,    S_PLUS);
    add_transition(S_START, C_MINUS,   S_MINUS);
    add_transition(S_START, C_LPAREN,  S_LPAREN);
    add_transition(S_START, C_RPAREN,  S_RPAREN);
    add_transition(S_START, C_LBRACE,  S_LBRACE);
    add_transition(S_START, C_RBRACE,  S_RBRACE);
    add_transition(S_START, C_COMMA,   S_COMMA);
    add_transition(S_START, C_SEMI,    S_SEMI);
    add_transition(S_START, C_SPACE,   S_WS);
    add_transition(S_START, C_NEWLINE, S_WS);

    // transitions for identifier/keyword
    add_transition(S_ID, C_LETTER, S_ID);
    add_transition(S_ID, C_DIGIT,  S_ID);
    mark_accepting(S_ID, TOK_IDENTIFIER);

    // transitions for integer literal
    add_transition(S_NUM, C_DIGIT, S_NUM);
    mark_accepting(S_NUM, TOK_INT_LITERAL);

    // transitions for character literal
    add_transition_except(S_QUOTE1, S_CHARVAL, {C_QUOTE, C_NEWLINE});
    add_transition(S_CHARVAL, C_QUOTE, S_CHARDONE);
    mark_accepting(S_CHARDONE, TOK_CHAR_LITERAL);

    // transitions for '=' and '=='
    mark_accepting(S_EQ1, TOK_ASSIGN);
    add_transition(S_EQ1, C_EQ, S_EQEQ);
    mark_accepting(S_EQEQ, TOK_EQ);

    // transitions for '<' and '<='
    mark_accepting(S_LT1, TOK_LT);
    add_transition(S_LT1, C_EQ, S_LE);
    mark_accepting(S_LE, TOK_LE);

    // transitions for '>' and '>='
    mark_accepting(S_GT1, TOK_GT);
    add_transition(S_GT1, C_EQ, S_GE);
    mark_accepting(S_GE, TOK_GE);

    // transitions for '!' and '!='
    mark_accepting(S_NOT1, TOK_NOT);
    add_transition(S_NOT1, C_EQ, S_NE);
    mark_accepting(S_NE, TOK_NE);

    // transition for &&
    // we dont consider a bitwise operator & in this language (for simplicity)
    add_transition(S_AND1, C_AMP, S_ANDAND);
    mark_accepting(S_ANDAND, TOK_AND);


    // transitionoin for ||
    // we dont consider a bitwise operator | in this language (for simplicity)
    add_transition(S_OR1, C_PIPE, S_OROR);
    mark_accepting(S_OROR, TOK_OR);

    // transition for comments and divide operator
    mark_accepting(S_SLASH1, TOK_DIVIDE);
    add_transition(S_SLASH1, C_SLASH, S_COMMENT);

    // comments consume everything up to new line
    add_transition_except(S_COMMENT, S_COMMENT, {C_NEWLINE});
    mark_accepting(S_COMMENT, TOK_COMMENT);

    // transitions for whitespace
    add_transition(S_WS, C_SPACE, S_WS);
    add_transition(S_WS, C_NEWLINE, S_WS);
    mark_accepting(S_WS, TOK_WHITESPACE);

    // transitions for single character tokens
    mark_accepting(S_PLUS, TOK_PLUS);
    mark_accepting(S_MINUS, TOK_MINUS);
    mark_accepting(S_STAR, TOK_STAR);
    mark_accepting(S_LPAREN, TOK_LPAREN);
    mark_accepting(S_RPAREN, TOK_RPAREN);
    mark_accepting(S_LBRACE, TOK_LBRACE);
    mark_accepting(S_RBRACE, TOK_RBRACE);
    mark_accepting(S_COMMA, TOK_COMMA);
    mark_accepting(S_SEMI, TOK_SEMI);

}

void lexer::advance(int from, int to){
    for (int i = from; i < to; i++){
        if (buffer[i] == '\n'){
            line++;
            col = 1;
        } else {
            col++;
        }
    }

    pos = to;
}

lexer::lexer(const std::string &source){
    buffer = source;
    pos = 0;
    line = 1;
    col = 1;
}

token lexer::next_token(){
    while (true){
        if (pos >= (int)buffer.size()){
            return {TOK_EOF, "", line, col};
        }

        int start_pos = pos;
        int start_line = line;

        int start_col = col;

        state current_state = S_START;

        int last_accepting_pos = -1;
        state last_accepting_state = S_START;
        int scan_pos = pos;

        while (scan_pos < (int)buffer.size()){
            char c = buffer[scan_pos];
            char_type ctype = classify_char(c);

            int next = table[current_state][ctype];

            // there is some invalid transition that
            // isnt defined
            // this happened due to some error
            if (next == -1){
                break;
            }

            current_state = (state)next;

            scan_pos++;

            // if current state is acecpting
            // then we record it
            // this is done to match the longest accepting state
            if (is_accepting[current_state]){
                last_accepting_pos = scan_pos;
                last_accepting_state = current_state;
            }
        }

        if (last_accepting_pos == -1){
            // no accepting state was found, this means the character at
            // start_pos is invalid, so we return an error token
            char invalid_char = buffer[start_pos];
            std::cerr<<"error: invalid character at line "<<line<<", col "<<col<<": '"<<invalid_char<<"'"<<std::endl;
            advance(start_pos, start_pos + 1);
        } else {
            // we have found an accepting state, so we return the token
            // the "retract" functionality also needs to be implemented here
            // well it is done automatically due to the way we are scanning the input stream
            std::string lexeme = buffer.substr(start_pos, last_accepting_pos - start_pos);
            token_type type = accepting_token[last_accepting_state];
            advance(start_pos, last_accepting_pos);

            if (type == TOK_WHITESPACE || type == TOK_COMMENT){
                continue;  // skip whitespace and comments
            }

            // if the identifier is a keyword, change its type to TOK_KEYWORD
            if (type == TOK_IDENTIFIER && (keywords.count(lexeme) > 0)){
                type = TOK_KEYWORD;
            }

            return token{type, lexeme, start_line, start_col};
        }
    }
}
