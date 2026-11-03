#pragma once
#include <map>
#include <set>
#include <string>
#include <vector>

extern const std::string EPSILON;   // "ε"
extern const std::string END_MARK;  // "$"

struct Production {
    std::string left_side;
    std::vector<std::string> right_side;  // empty vector means  left_side -> ε
};

class Grammar {
public:
    // reads the grammar file returns false and fills `error_message` on failure
    bool load_file(const std::string& file_path, std::string& error_message);

    // Computes the first and follow sets
    void compute_sets();

    // FIRST of a sequence of symbols; sets `is_nullable` if the whole sequence can derive ε.
    std::set<std::string> first_of_sequence(const std::vector<std::string>& symbols,
                                            bool& is_nullable) const;

    bool is_nonterminal(const std::string& symbol) const {
        return nonterminal_set.count(symbol) > 0;
    }

    const std::string& get_start() const { return start_symbol; }
    const std::vector<Production>& get_productions() const { return production_list; }
    const std::vector<std::string>& get_nonterminals() const { return nonterminal_list; }
    const std::set<std::string>& get_terminals() const { return terminal_set; }
    const std::set<std::string>& get_nullable() const { return nullable_set; }
    const std::map<std::string, std::set<std::string>>& get_first() const { return first_sets; }
    const std::map<std::string, std::set<std::string>>& get_follow() const { return follow_sets; }

private:
    std::vector<Production> production_list;
    std::vector<std::string> nonterminal_list;   // in order of first appearance
    std::set<std::string> nonterminal_set;
    std::set<std::string> terminal_set;
    std::string start_symbol;
    std::set<std::string> nullable_set;
    std::map<std::string, std::set<std::string>> first_sets;   // never contains ε
    std::map<std::string, std::set<std::string>> follow_sets;
};
