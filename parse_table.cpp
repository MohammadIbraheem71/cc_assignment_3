#include "parse_table.h"

#include <iostream>

void ParseTable::build(const Grammar& grammar) {
    table_entries.clear();
    conflict_list.clear();

    const std::vector<Production>& production_list = grammar.get_productions();

    // for each production: left side -> right side
    for (size_t i = 0; i < production_list.size(); ++i) {
        const Production& production = production_list[i];
        int production_number = static_cast<int>(i) + 1;  // same numbering as the printed productions

        // first of the whole right side, and whether the right side can derive nothing (ε)
        bool is_nullable;
        std::set<std::string> first_set =
            grammar.first_of_sequence(production.right_side, is_nullable);

        // step 1: every terminal in the first set selects this production
        for (const std::string& terminal : first_set) {
            add_entry(production.left_side, terminal, production_number);
        }

        // step 2: if the right side can derive ε, every terminal in the follow set of the
        // left side selects this production too (the end marker $ is in that set as well)
        if (is_nullable) {
            for (const std::string& terminal : grammar.get_follow().at(production.left_side)) {
                add_entry(production.left_side, terminal, production_number);
            }
        }
    }

    // step 3: any cell that was never written stays empty, and get_entry returns 0 for it
}

void ParseTable::add_entry(const std::string& nonterminal, const std::string& terminal,
                           int production_number) {
    std::map<std::string, int>& row = table_entries[nonterminal];

    auto found = row.find(terminal);
    if (found != row.end()) {
        // the same production writing the same cell twice is fine
        // a different production means the grammar is not ll(1)
        if (found->second != production_number) {
            conflict_list.push_back("conflict at [" + nonterminal + ", " + terminal +
                                    "]: production " + std::to_string(found->second) +
                                    " and production " + std::to_string(production_number));
        }
        return;
    }
    row[terminal] = production_number;
}

int ParseTable::get_entry(const std::string& nonterminal, const std::string& terminal) const {
    auto row_found = table_entries.find(nonterminal);
    if (row_found == table_entries.end()) return 0;

    auto cell_found = row_found->second.find(terminal);
    if (cell_found == row_found->second.end()) return 0;

    return cell_found->second;
}

void ParseTable::print(const Grammar& grammar) const {
    // columns are all the terminals, then the end marker
    std::vector<std::string> column_list(grammar.get_terminals().begin(),
                                         grammar.get_terminals().end());
    column_list.push_back(END_MARK);

    for (const std::string& nonterminal : grammar.get_nonterminals()) {
        std::cout << "  " << nonterminal << ":";
        for (const std::string& terminal : column_list) {
            int production_number = get_entry(nonterminal, terminal);
            if (production_number != 0) {
                std::cout << " " << terminal << "=" << production_number;
            }
        }
        std::cout << "\n";
    }

    if (conflict_list.empty()) {
        std::cout << "\nconflicts: none, the grammar is ll(1)\n";
    } else {
        std::cout << "\nconflicts found:\n";
        for (const std::string& message : conflict_list) {
            std::cout << "  " << message << "\n";
        }
    }
}
