#include "parser.h"

#include <iostream>

static TreeNode* new_node(const std::string& symbol) {
    TreeNode* node = new TreeNode;
    node->symbol = symbol;
    return node;
}

// the end marker is shown as "end of file" in messages
static std::string get_display_name(const std::string& symbol) {
    if (symbol == END_MARK) return "end of file";
    return symbol;
}

Parser::Parser(const Grammar& source_grammar, const ParseTable& source_table)
    : grammar(source_grammar), parse_table(source_table) {}

std::string Parser::get_expected_tokens(const std::string& nonterminal) const {
    std::string expected_text;
    for (const std::string& terminal : grammar.get_terminals()) {
        if (parse_table.get_entry(nonterminal, terminal) != 0) {
            if (!expected_text.empty()) expected_text += ", ";
            expected_text += terminal;
        }
    }
    // the end marker is not in the terminal set, so check it separately
    if (parse_table.get_entry(nonterminal, END_MARK) != 0) {
        if (!expected_text.empty()) expected_text += ", ";
        expected_text += get_display_name(END_MARK);
    }
    return expected_text;
}

TreeNode* Parser::fail(const token& current_token, const std::string& expected_text,
                       TreeNode* root, TreeNode* end_node) {
    std::string found_text = "end of file";
    if (current_token.type != TOK_EOF) found_text = "'" + current_token.lexeme + "'";

    error_message = "syntax error at line " + std::to_string(current_token.line) +
                    ", col " + std::to_string(current_token.column) +
                    ": found " + found_text + ", expected: " + expected_text;

    delete_tree(root);
    delete end_node;
    return nullptr;
}

TreeNode* Parser::parse(lexer& lex) {
    error_message = "";

    TreeNode* end_node = new_node(END_MARK);
    TreeNode* root = new_node(grammar.get_start());

    // the stack holds the nodes still waiting to be processed, the top is the last element
    std::vector<TreeNode*> stack;
    stack.push_back(end_node);
    stack.push_back(root);

    // the current token is the one token of lookahead
    token current_token = lex.next_token();
    std::string current_name = get_terminal_name(current_token);

    while (true) {
        TreeNode* top = stack.back();
        stack.pop_back();

        if (!grammar.is_nonterminal(top->symbol)) {
            // a terminal (or the end marker) must match the current token
            if (top->symbol != current_name) {
                return fail(current_token, get_display_name(top->symbol), root, end_node);
            }

            // the end marker matched the end of the input, so we are done
            if (top->symbol == END_MARK) break;

            // a normal terminal: remember its text and read the next token
            top->lexeme = current_token.lexeme;
            current_token = lex.next_token();
            current_name = get_terminal_name(current_token);
        } else {
            // a nonterminal: the table tells us which production to use
            int production_number = parse_table.get_entry(top->symbol, current_name);
            if (production_number == 0) {
                return fail(current_token, get_expected_tokens(top->symbol), root, end_node);
            }
            const Production& production = grammar.get_productions()[production_number - 1];

            if (production.right_side.empty()) {
                // an empty production: show it in the tree with an epsilon leaf
                top->children.push_back(new_node(EPSILON));
            } else {
                // make one child for each symbol on the right side
                for (const std::string& symbol : production.right_side) {
                    top->children.push_back(new_node(symbol));
                }
                // push them in reverse so the first symbol ends up on top of the stack
                for (int i = static_cast<int>(top->children.size()) - 1; i >= 0; --i) {
                    stack.push_back(top->children[i]);
                }
            }
        }
    }

    delete end_node;
    return root;
}

void print_tree(const TreeNode* node, int depth) {
    std::cout << std::string(depth * 2, ' ') << node->symbol;
    // show the text of a terminal when it adds information, like id (index)
    if (!node->lexeme.empty() && node->lexeme != node->symbol) {
        std::cout << " (" << node->lexeme << ")";
    }
    std::cout << "\n";

    for (const TreeNode* child : node->children) {
        print_tree(child, depth + 1);
    }
}

void delete_tree(TreeNode* node) {
    for (TreeNode* child : node->children) {
        delete_tree(child);
    }
    delete node;
}
