#pragma once
#include <map>
#include <string>
#include <vector>

// the kinds of names that can be declared
enum SymbolKind {
    SYMBOL_VARIABLE,
    SYMBOL_PARAMETER,
    SYMBOL_FUNCTION
};

// the value types used by the c-- language, plus an error type
enum ValueType {
    TYPE_INT,
    TYPE_CHAR,
    TYPE_ERROR
};

// one row of the symbol table
struct SymbolEntry {
    std::string name;
    SymbolKind kind = SYMBOL_VARIABLE;
    ValueType type = TYPE_ERROR;
    std::vector<ValueType> parameter_types;   // only used for functions
    std::string unique_name;
};

class SymbolTable {
public:
    // the constructor starts the global scope
    SymbolTable();

    void enter_scope();

    // the global scope is never removed
    void exit_scope();

    // declares a name in the current scope, false means it is already there.
    // a succesful declaration also fills in entry.unique_name
    bool declare(SymbolEntry& entry);

    // searches from the innermost scope outward, nullptr if not found
    const SymbolEntry* lookup(const std::string& name) const;

private:
    // a stack of maps
    // each map represents a scope
    // a scope is popped when it is exited
    std::vector<std::map<std::string, SymbolEntry>> scope_stack;
    std::map<std::string, int> name_counter;
};
