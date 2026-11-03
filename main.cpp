#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "grammar.h"
#include "lexer.h"
#include "parse_table.h"
#include "parser.h"

// prints a set like { a, b, c }
static void print_set(const std::set<std::string>& symbol_set) {
    std::cout << "{ ";
    bool is_first_item = true;
    for (const std::string& symbol : symbol_set) {
        if (!is_first_item) std::cout << ", ";
        std::cout << symbol;
        is_first_item = false;
    }
    std::cout << " }";
}

static void print_productions(const Grammar& grammar) {
    std::cout << "Productions\n";
    int production_number = 1;
    for (const Production& production : grammar.get_productions()) {
        std::cout << "  " << production_number++ << ". " << production.left_side << " ->";
        if (production.right_side.empty()) std::cout << " " << EPSILON;
        for (const std::string& symbol : production.right_side) std::cout << " " << symbol;
        std::cout << "\n";
    }
}

static void print_first_sets(const Grammar& grammar) {
    std::cout << "FIRST sets\n";
    for (const std::string& nonterminal : grammar.get_nonterminals()) {
        std::set<std::string> first_set = grammar.get_first().at(nonterminal);
        // epsilon is not stored in the first sets, so add it here for nullable symbols
        if (grammar.get_nullable().count(nonterminal)) first_set.insert(EPSILON);
        std::cout << "  FIRST(" << nonterminal << ") = ";
        print_set(first_set);
        std::cout << "\n";
    }
}

static void print_follow_sets(const Grammar& grammar) {
    std::cout << "FOLLOW sets\n";
    for (const std::string& nonterminal : grammar.get_nonterminals()) {
        std::cout << "  FOLLOW(" << nonterminal << ") = ";
        print_set(grammar.get_follow().at(nonterminal));
        std::cout << "\n";
    }
}

int main() {
    const std::string grammar_path = "grammar.txt";
    const std::string source_path = "code.cmm";

    // set this to true to also print the productions, first/follow sets and the parse table
    const bool show_grammar_info = false;

    // read the grammar from the file and compute nullable, first and follow
    Grammar grammar;
    std::string error_message;
    if (!grammar.load_file(grammar_path, error_message)) {
        std::cerr << "Grammar error: " << error_message << "\n";
        return 1;
    }
    grammar.compute_sets();

    // build the parse table, a conflict means the grammar is not ll(1)
    ParseTable parse_table;
    parse_table.build(grammar);
    if (!parse_table.get_conflicts().empty()) {
        std::cerr << "the grammar is not ll(1):\n";
        for (const std::string& message : parse_table.get_conflicts()) {
            std::cerr << "  " << message << "\n";
        }
        return 1;
    }

    if (show_grammar_info) {
        std::cout << "Start symbol: " << grammar.get_start() << "\n\n";
        print_productions(grammar);
        std::cout << "\nTerminals: ";
        print_set(grammar.get_terminals());
        std::cout << "\n\nNullable: ";
        print_set(grammar.get_nullable());
        std::cout << "\n\n";
        print_first_sets(grammar);
        std::cout << "\n";
        print_follow_sets(grammar);
        std::cout << "\nParse table (terminal=production number)\n";
        parse_table.print(grammar);
        std::cout << "\n";
    }

    // read the source program
    std::ifstream source_file(source_path);
    if (!source_file.is_open()) {
        std::cerr << "error: could not open file " << source_path << "\n";
        return 1;
    }
    std::stringstream source_stream;
    source_stream << source_file.rdbuf();

    // the lexer must have its transition table built before it is used
    build_transition_table();
    lexer lex(source_stream.str());

    // the parser asks the lexer for tokens while it builds the parse tree
    Parser parser(grammar, parse_table);
    TreeNode* root = parser.parse(lex);

    if (root == nullptr) {
        std::cerr << parser.get_error() << "\n";
        return 1;
    }

    std::cout << "parse successful, parse tree:\n";
    print_tree(root, 0);
    delete_tree(root);
    return 0;
}
