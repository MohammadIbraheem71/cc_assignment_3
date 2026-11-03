#pragma once
#include <map>
#include <string>
#include <vector>

#include "grammar.h"

class ParseTable {
public:
    // fills the table from a grammar whose sets are already computed
    void build(const Grammar& grammar);

    // returns the production number, or 0 if the cell is empty (syntax error)
    int get_entry(const std::string& nonterminal, const std::string& terminal) const;

    const std::vector<std::string>& get_conflicts() const { return conflict_list; }

    // prints every non empty cell, then the conflicts (if any)
    void print(const Grammar& grammar) const;

private:
    // writes one cell, or records a conflict if the cell already holds another production
    void add_entry(const std::string& nonterminal, const std::string& terminal,
                   int production_number);

    // table_entries[nonterminal][terminal] = production number
    std::map<std::string, std::map<std::string, int>> table_entries;
    std::vector<std::string> conflict_list;
};
