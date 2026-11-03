#pragma once
#include <string>
#include <vector>

#include "grammar.h"
#include "lexer.h"
#include "parse_table.h"

// one node of the parse tree
struct TreeNode {
    std::string symbol;                 // grammar symbol, such as a nonterminal name or id
    std::string lexeme;                 // the matched text, only filled for terminals
    int line = 0;                       // source line, filled for terminals
    int column = 0;                     // source column, filled for terminals
    std::vector<TreeNode*> children;
};

class Parser {
public:
    Parser(const Grammar& source_grammar, const ParseTable& source_table);

    // reads tokens from the lexer and returns the root of the parse tree
    // returns nullptr if there is a syntax error (see get_error)
    TreeNode* parse(lexer& lex);

    const std::string& get_error() const { return error_message; }

private:
    // lists the terminals that have an entry in the row of a nonterminal
    std::string get_expected_tokens(const std::string& nonterminal) const;

    // saves the error message, frees the tree built so far and returns nullptr
    TreeNode* fail(const token& current_token, const std::string& expected_text,
                   TreeNode* root, TreeNode* end_node);

    const Grammar& grammar;
    const ParseTable& parse_table;
    std::string error_message;
};

// prints the tree, indented by depth
void print_tree(const TreeNode* node, int depth);

// returns the first terminal below a node, or nullptr if there is none
const TreeNode* find_first_terminal(const TreeNode* node);

// frees a node and everything below it
void delete_tree(TreeNode* node);
