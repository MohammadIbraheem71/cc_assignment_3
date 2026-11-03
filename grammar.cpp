#include "grammar.h"

#include <fstream>
#include <sstream>

const std::string EPSILON = "ε";
const std::string END_MARK = "$";

static bool is_epsilon_token(const std::string& word) {
    return word == "ε" || word == "eps" || word == "epsilon";
}

static bool is_arrow_token(const std::string& word) {
    return word == "->" || word == "→" || word == "::=";
}

static std::vector<std::string> split_words(const std::string& text) {
    std::istringstream input(text);
    std::vector<std::string> word_list;
    std::string word;
    while (input >> word) word_list.push_back(word);
    return word_list;
}

bool Grammar::load_file(const std::string& file_path, std::string& error_message) {
    std::ifstream grammar_file(file_path);
    if (!grammar_file) {
        error_message = "cannot open grammar file '" + file_path + "'";
        return false;
    }

    std::string line, current_left_side;
    int line_number = 0;

    // this loop runs for each line of the grammar
    while (std::getline(grammar_file, line)) {
        ++line_number;
        size_t comment_start = line.find('#');           // strip comments

        if (comment_start != std::string::npos){
            line.erase(comment_start);
        }

        std::vector<std::string> word_list = split_words(line);

        if (word_list.empty()){
            continue;
        }

        size_t first_index = 1;                          // index where the alternatives start
        if (word_list[0] == "|") {                       // continuation of previous rule
            if (current_left_side.empty()) {
                error_message = "line " + std::to_string(line_number) + ": '|' before any rule";
                return false;
            }
        } else {
            if (word_list.size() < 3 || !is_arrow_token(word_list[1])) {
                error_message = "line " + std::to_string(line_number) + ": expected  LHS -> ...";
                return false;
            }
            current_left_side = word_list[0];
            if (!nonterminal_set.count(current_left_side)) {
                nonterminal_set.insert(current_left_side);
                nonterminal_list.push_back(current_left_side);
            }
            if (start_symbol.empty()){
                start_symbol = current_left_side;
            }
            first_index = 2;
        }

        // now we split the remaining words into alternatives separated by "|"
        std::vector<std::string> alternative;
        bool has_symbols = false;

        auto flush_alternative = [&]() {
            Production production;
            production.left_side = current_left_side;
            if (!(alternative.size() == 1 && is_epsilon_token(alternative[0]))){
                production.right_side = alternative;
            }
            production_list.push_back(production);
            alternative.clear();
            has_symbols = false;
        };

        for (size_t i = first_index; i < word_list.size(); ++i) {
            if (word_list[i] == "|") {
                if (!has_symbols) {
                    error_message = "line " + std::to_string(line_number) +
                                    ": empty alternative (use ε)";
                    return false;
                }
                flush_alternative();
            } else {
                alternative.push_back(word_list[i]);
                has_symbols = true;
            }
        }
        if (!has_symbols) {
            error_message = "line " + std::to_string(line_number) + ": empty alternative (use ε)";
            return false;
        }
        flush_alternative();
    }

    if (production_list.empty()) {
        error_message = "grammar file has no rules";
        return false;
    }

    // Terminals = symbols used on a right-hand side that are never a left-hand side.
    for (const Production& production : production_list){
        for (const std::string& symbol : production.right_side){
            if (!nonterminal_set.count(symbol)){
                terminal_set.insert(symbol);
            }
        }
    }
    return true;
}

std::set<std::string> Grammar::first_of_sequence(const std::vector<std::string>& symbols,
                                                 bool& is_nullable) const {
    std::set<std::string> result;
    is_nullable = true;
    for (const std::string& symbol : symbols) {
        if (!is_nonterminal(symbol)) {                   // terminal: stop here
            result.insert(symbol);
            is_nullable = false;
            return result;
        }
        const std::set<std::string>& symbol_first = first_sets.at(symbol);
        result.insert(symbol_first.begin(), symbol_first.end());
        if (!nullable_set.count(symbol)) {               // this symbol can't vanish: stop
            is_nullable = false;
            return result;
        }
    }
    return result;                                       // every symbol was nullable
}

void Grammar::compute_sets() {
    nullable_set.clear();
    first_sets.clear();
    follow_sets.clear();
    for (const std::string& nonterminal : nonterminal_list) {
        first_sets[nonterminal];
        follow_sets[nonterminal];
    }

    // 1. Nullable: A is nullable if some production has an all-nullable (or empty) right side
    for (bool changed = true; changed;) {
        changed = false;
        for (const Production& production : production_list) {
            if (nullable_set.count(production.left_side)) continue;
            bool all_nullable = true;
            for (const std::string& symbol : production.right_side)
                if (!nullable_set.count(symbol)) { all_nullable = false; break; }  // terminals never nullable
            if (all_nullable) {
                nullable_set.insert(production.left_side);
                changed = true;
            }
        }
    }

    // we calculate first, this setep is repeated until nothing changes
    for (bool changed = true; changed;) {
        changed = false;
        for (const Production& production : production_list) {
            bool is_nullable;
            std::set<std::string> new_first = first_of_sequence(production.right_side, is_nullable);
            std::set<std::string>& target_set = first_sets[production.left_side];
            size_t old_size = target_set.size();
            target_set.insert(new_first.begin(), new_first.end());
            if (target_set.size() != old_size) changed = true;
        }
    }

    // we calculate follow, start symbol gets the dollar sign, propogated until nothing changes
    follow_sets[start_symbol].insert(END_MARK);
    for (bool changed = true; changed;) {
        changed = false;
        // run this loop for each production in production_list
        for (const Production& production : production_list) {
            const std::vector<std::string>& right_side = production.right_side;

            // run this loop for each word on the right side
            for (size_t i = 0; i < right_side.size(); ++i) {
                const std::string& current_symbol = right_side[i];
                if (!is_nonterminal(current_symbol)){
                    continue;
                }
                std::vector<std::string> rest_symbols(right_side.begin() + i + 1, right_side.end());
                bool rest_nullable;
                std::set<std::string> to_add = first_of_sequence(rest_symbols, rest_nullable);
                if (rest_nullable) {                     // current symbol can end the production
                    const std::set<std::string>& left_follow = follow_sets[production.left_side];
                    to_add.insert(left_follow.begin(), left_follow.end());
                }
                std::set<std::string>& target_set = follow_sets[current_symbol];
                size_t old_size = target_set.size();
                target_set.insert(to_add.begin(), to_add.end());
                if (target_set.size() != old_size) changed = true;
            }
        }
    }
}
