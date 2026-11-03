#include "analyzer.h"

#include <string>
#include <vector>

// one parameter found in a param list, with its id node for error possitions
struct ParamInfo {
    std::string name;
    ValueType type;
    TreeNode* id_node;
};

// an epsilon node has exactly one child named epsilon
static bool is_epsilon_node(const TreeNode* node) {
    if (node == nullptr) return false;
    if (node->symbol == "ε") return true;
    return node->children.size() == 1 && node->children[0]->symbol == "ε";
}

// a type node has one child, either int or char
static ValueType type_from_node(const TreeNode* type_node) {
    if (type_node->children[0]->symbol == "int") {
        return TYPE_INT;
    }
    return TYPE_CHAR;
}

static std::string type_name(ValueType type) {
    if (type == TYPE_INT) return "int";
    if (type == TYPE_CHAR) return "char";
    return "error";
}

// char can become int implicitely, int can not become char
static bool is_assignable(ValueType from, ValueType to) {
    // an error type means a message was allready printed, avoid a cascade
    if (from == TYPE_ERROR || to == TYPE_ERROR) return true;
    if (from == to) return true;
    if (from == TYPE_CHAR && to == TYPE_INT) return true;
    return false;
}

static std::string terminal_to_operator(const std::string& terminal) {
    if (terminal == "or") return "||";
    if (terminal == "and") return "&&";
    if (terminal == "lt") return "<";
    if (terminal == "gt") return ">";
    if (terminal == "le") return "<=";
    if (terminal == "ge") return ">=";
    if (terminal == "eq") return "==";
    if (terminal == "ne") return "!=";
    if (terminal == "plus") return "+";
    if (terminal == "minus") return "-";
    if (terminal == "star") return "*";
    if (terminal == "slash") return "/";
    if (terminal == "percent") return "%";
    return "?";
}

static void add_param_info(TreeNode* param, std::vector<ParamInfo>& params) {
    ParamInfo info;
    info.type = type_from_node(param->children[0]);
    info.id_node = param->children[1];
    info.name = info.id_node->lexeme;
    params.push_back(info);
}

// param list -> param param tail | ε, param tail -> comma param param tail | ε
static void collect_param_info(TreeNode* param_list, std::vector<ParamInfo>& params) {
    if (is_epsilon_node(param_list)) return;

    add_param_info(param_list->children[0], params);

    TreeNode* tail = param_list->children[1];
    while (!is_epsilon_node(tail)) {
        add_param_info(tail->children[1], params);
        tail = tail->children[2];
    }
}

Analyzer::Analyzer(TreeNode* root)
    : root(root), temporary_count(0), label_count(0), current_return_type(TYPE_ERROR) {}

void Analyzer::analyze() {
    if (root == nullptr) return;

    // find every function name first, so a call before the defenition can be
    // told apart from a completely unknown function
    collect_function_signatures(root);
    analyze_program(root);
}

void Analyzer::collect_function_signatures(TreeNode* program) {
    TreeNode* current = program;

    while (current != nullptr && current->symbol == "Program" && !is_epsilon_node(current)) {
        TreeNode* decl = current->children[0];
        TreeNode* rest = decl->children[2];

        // only function declarations have an external rest that starts with lparen
        if (!rest->children.empty() && rest->children[0]->symbol == "lparen") {
            SymbolEntry entry;
            entry.name = decl->children[1]->lexeme;
            entry.kind = SYMBOL_FUNCTION;
            entry.type = type_from_node(decl->children[0]);

            std::vector<ParamInfo> params;
            collect_param_info(rest->children[1], params);
            for (size_t i = 0; i < params.size(); ++i) {
                entry.parameter_types.push_back(params[i].type);
            }

            function_signatures[entry.name] = entry;
        }

        current = current->children[1];
    }
}

void Analyzer::analyze_program(TreeNode* program) {
    TreeNode* current = program;

    while (current != nullptr && current->symbol == "Program" && !is_epsilon_node(current)) {
        analyze_external_decl(current->children[0]);
        current = current->children[1];
    }
}

void Analyzer::analyze_external_decl(TreeNode* decl) {
    ValueType type = type_from_node(decl->children[0]);
    TreeNode* id_node = decl->children[1];
    TreeNode* rest = decl->children[2];

    if (rest->children[0]->symbol == "lparen") {
        // type id lparen param list rparen block
        analyze_function(id_node, type, rest->children[1], rest->children[3]);
    } else {
        // type id id tail semi, a list of global variables
        declare_variable(id_node, type);
        declare_id_tail(type, rest->children[0]);
    }
}

void Analyzer::analyze_function(TreeNode* id_node, ValueType return_type,
                                TreeNode* param_list, TreeNode* block) {
    std::vector<ParamInfo> params;
    collect_param_info(param_list, params);

    SymbolEntry entry;
    entry.name = id_node->lexeme;
    entry.kind = SYMBOL_FUNCTION;
    entry.type = return_type;
    entry.unique_name = id_node->lexeme;   // fallback if the declaration fails
    for (size_t i = 0; i < params.size(); ++i) {
        entry.parameter_types.push_back(params[i].type);
    }

    // the function is declared before its body, so recursion works
    if (!symbol_table.declare(entry)) {
        report_error(id_node, "redeclaration of '" + entry.name + "' in the same scope");
    }

    emit("func " + entry.unique_name + ":");

    // parameters and the body top level share one scope
    symbol_table.enter_scope();
    for (size_t i = 0; i < params.size(); ++i) {
        SymbolEntry param_entry;
        param_entry.name = params[i].name;
        param_entry.kind = SYMBOL_PARAMETER;
        param_entry.type = params[i].type;
        if (!symbol_table.declare(param_entry)) {
            report_error(params[i].id_node,
                         "redeclaration of '" + param_entry.name + "' in the same scope");
        }
    }

    ValueType previous_return_type = current_return_type;
    current_return_type = return_type;
    analyze_block(block, false);
    current_return_type = previous_return_type;

    symbol_table.exit_scope();
    emit("endfunc");
}

void Analyzer::declare_variable(TreeNode* id_node, ValueType type) {
    SymbolEntry entry;
    entry.name = id_node->lexeme;
    entry.kind = SYMBOL_VARIABLE;
    entry.type = type;

    if (!symbol_table.declare(entry)) {
        report_error(id_node, "redeclaration of '" + entry.name + "' in the same scope");
    }
}

// id tail -> comma id id tail | ε
void Analyzer::declare_id_tail(ValueType type, TreeNode* id_tail) {
    TreeNode* tail = id_tail;

    while (!is_epsilon_node(tail)) {
        declare_variable(tail->children[1], type);
        tail = tail->children[2];
    }
}

void Analyzer::analyze_block(TreeNode* block, bool open_scope) {
    if (block == nullptr || block->children.size() < 2) return;

    if (open_scope) {
        symbol_table.enter_scope();
    }

    // block -> lbrace stmt list rbrace
    analyze_stmt_list(block->children[1]);

    if (open_scope) {
        symbol_table.exit_scope();
    }
}

void Analyzer::analyze_stmt_list(TreeNode* stmt_list) {
    TreeNode* current = stmt_list;

    while (current != nullptr && current->symbol == "StmtList" && !is_epsilon_node(current)) {
        analyze_stmt(current->children[0]);
        current = current->children[1];
    }
}

void Analyzer::analyze_stmt(TreeNode* stmt) {
    if (stmt->children.empty()) return;

    std::string first_symbol = stmt->children[0]->symbol;

    if (first_symbol == "Block") {
        // stmt -> block, this is a nested block and it gets its own scope
        analyze_block(stmt->children[0], true);
        return;
    }

    if (first_symbol == "Type") {
        // stmt -> type id id tail semi
        ValueType type = type_from_node(stmt->children[0]);
        declare_variable(stmt->children[1], type);
        declare_id_tail(type, stmt->children[2]);
        return;
    }

    if (first_symbol == "id") {
        // stmt -> id stmt rest
        analyze_assignment_or_call(stmt->children[0], stmt->children[1]);
        return;
    }

    if (first_symbol == "if") {
        // stmt -> if lparen expr rparen block else part
        ExprResult condition = analyze_expr(stmt->children[2]);
        TreeNode* then_block = stmt->children[4];
        TreeNode* else_part = stmt->children[5];

        if (is_epsilon_node(else_part)) {
            std::string end_label = make_label();
            if (condition.type != TYPE_ERROR) {
                emit("if_false " + condition.place + " goto " + end_label);
            }
            analyze_block(then_block, true);
            emit(end_label + ":");
        } else {
            std::string else_label = make_label();
            std::string end_label = make_label();

            if (condition.type != TYPE_ERROR) {
                emit("if_false " + condition.place + " goto " + else_label);
            }
            analyze_block(then_block, true);
            emit("goto " + end_label);
            emit(else_label + ":");
            analyze_block(else_part->children[1], true);
            emit(end_label + ":");
        }
        return;
    }

    if (first_symbol == "while") {
        // stmt -> while lparen expr rparen block
        std::string start_label = make_label();
        std::string end_label = make_label();

        emit(start_label + ":");
        ExprResult condition = analyze_expr(stmt->children[2]);
        if (condition.type != TYPE_ERROR) {
            emit("if_false " + condition.place + " goto " + end_label);
        }
        analyze_block(stmt->children[4], true);
        emit("goto " + start_label);
        emit(end_label + ":");
        return;
    }

    if (first_symbol == "return") {
        // stmt -> return expr semi
        ExprResult value = analyze_expr(stmt->children[1]);
        if (!is_assignable(value.type, current_return_type)) {
            report_error(stmt->children[1],
                         "cannot return '" + type_name(value.type) + "' from a function returning '" +
                             type_name(current_return_type) + "'");
        }
        if (value.type != TYPE_ERROR) {
            emit("return " + value.place);
        }
        return;
    }
}

void Analyzer::analyze_assignment_or_call(TreeNode* id_node, TreeNode* stmt_rest) {
    const SymbolEntry* entry = symbol_table.lookup(id_node->lexeme);
    TreeNode* first = stmt_rest->children[0];

    if (first->symbol == "assign") {
        // stmt rest -> assign expr semi
        ExprResult value = analyze_expr(stmt_rest->children[1]);

        if (entry == nullptr) {
            report_error(id_node, "undeclared identifier '" + id_node->lexeme + "'");
            return;
        }

        if (entry->kind == SYMBOL_FUNCTION) {
            report_error(id_node, "'" + id_node->lexeme + "' is a function, not a variable");
            return;
        }

        if (!is_assignable(value.type, entry->type)) {
            report_error(id_node, "cannot assign '" + type_name(value.type) + "' to '" +
                                      type_name(entry->type) + "'");
        }

        if (value.type != TYPE_ERROR) {
            emit(entry->unique_name + " = " + value.place);
        }
        return;
    }

    // stmt rest -> lparen arg list rparen semi, a call statement
    analyze_call(id_node, stmt_rest->children[1], false);
}

ExprResult Analyzer::analyze_call(TreeNode* id_node, TreeNode* arg_list, bool use_result) {
    const SymbolEntry* entry = symbol_table.lookup(id_node->lexeme);

    if (entry == nullptr) {
        if (function_signatures.count(id_node->lexeme) > 0) {
            report_error(id_node, "function '" + id_node->lexeme +
                                      "' is called before it is defined");
        } else {
            report_error(id_node, "undefined function '" + id_node->lexeme + "'");
        }
        return error_result();
    }

    if (entry->kind != SYMBOL_FUNCTION) {
        report_error(id_node, "'" + id_node->lexeme + "' is a variable, not a function");
        return error_result();
    }

    std::vector<ExprResult> args;
    collect_arg_list(arg_list, args);

    const std::vector<ValueType>& parameter_types = entry->parameter_types;
    if (args.size() != parameter_types.size()) {
        report_error(id_node, "function '" + id_node->lexeme + "' expects " +
                                  std::to_string(parameter_types.size()) + " arguments but got " +
                                  std::to_string(args.size()));
    }

    size_t check_count = args.size();
    if (parameter_types.size() < check_count) {
        check_count = parameter_types.size();
    }
    for (size_t i = 0; i < check_count; ++i) {
        if (!is_assignable(args[i].type, parameter_types[i])) {
            report_error(id_node, "argument " + std::to_string(i + 1) + " of '" +
                                      id_node->lexeme + "' has type '" + type_name(args[i].type) +
                                      "', expected '" + type_name(parameter_types[i]) + "'");
        }
    }

    // emit all params left to right, then the call itself
    for (size_t i = 0; i < args.size(); ++i) {
        emit("param " + args[i].place);
    }

    std::string call_text = "call " + entry->unique_name + ", " + std::to_string(args.size());
    if (use_result) {
        std::string temp = make_temp();
        emit(temp + " = " + call_text);

        ExprResult result;
        result.place = temp;
        result.type = entry->type;
        return result;
    }

    emit(call_text);
    return error_result();
}

// arg list -> expr arg tail | ε, arg tail -> comma expr arg tail | ε
void Analyzer::collect_arg_list(TreeNode* arg_list, std::vector<ExprResult>& args) {
    if (is_epsilon_node(arg_list)) return;

    args.push_back(analyze_expr(arg_list->children[0]));

    TreeNode* tail = arg_list->children[1];
    while (!is_epsilon_node(tail)) {
        args.push_back(analyze_expr(tail->children[1]));
        tail = tail->children[2];
    }
}

// expr -> and expr or tail, the tail is folded from the left
ExprResult Analyzer::analyze_expr(TreeNode* expr) {
    ExprResult left = analyze_and_expr(expr->children[0]);
    TreeNode* tail = expr->children[1];

    while (!is_epsilon_node(tail)) {
        ExprResult right = analyze_and_expr(tail->children[1]);
        left = make_binary("||", left, right);
        tail = tail->children[2];
    }
    return left;
}

// and expr -> rel expr and tail, the tail is folded from the left
ExprResult Analyzer::analyze_and_expr(TreeNode* expr) {
    ExprResult left = analyze_rel_expr(expr->children[0]);
    TreeNode* tail = expr->children[1];

    while (!is_epsilon_node(tail)) {
        ExprResult right = analyze_rel_expr(tail->children[1]);
        left = make_binary("&&", left, right);
        tail = tail->children[2];
    }
    return left;
}

// rel expr -> add expr rel tail, the tail does not repeat
ExprResult Analyzer::analyze_rel_expr(TreeNode* expr) {
    ExprResult left = analyze_add_expr(expr->children[0]);
    TreeNode* tail = expr->children[1];

    if (!is_epsilon_node(tail)) {
        std::string op = terminal_to_operator(tail->children[0]->symbol);
        ExprResult right = analyze_add_expr(tail->children[1]);
        return make_binary(op, left, right);
    }
    return left;
}

// add expr -> term add tail, the tail is folded from the left
ExprResult Analyzer::analyze_add_expr(TreeNode* expr) {
    ExprResult left = analyze_term(expr->children[0]);
    TreeNode* tail = expr->children[1];

    while (!is_epsilon_node(tail)) {
        std::string op = terminal_to_operator(tail->children[0]->symbol);
        ExprResult right = analyze_term(tail->children[1]);
        left = make_binary(op, left, right);
        tail = tail->children[2];
    }
    return left;
}

// term -> factor mul tail, the tail is folded from the left
ExprResult Analyzer::analyze_term(TreeNode* expr) {
    ExprResult left = analyze_factor(expr->children[0]);
    TreeNode* tail = expr->children[1];

    while (!is_epsilon_node(tail)) {
        std::string op = terminal_to_operator(tail->children[0]->symbol);
        ExprResult right = analyze_factor(tail->children[1]);
        left = make_binary(op, left, right);
        tail = tail->children[2];
    }
    return left;
}

ExprResult Analyzer::analyze_factor(TreeNode* factor) {
    if (factor->children.empty()) return error_result();

    std::string first_symbol = factor->children[0]->symbol;

    if (first_symbol == "lparen") {
        // factor -> lparen expr rparen
        return analyze_expr(factor->children[1]);
    }

    if (first_symbol == "num") {
        ExprResult result;
        result.place = factor->children[0]->lexeme;
        result.type = TYPE_INT;
        return result;
    }

    if (first_symbol == "char_lit") {
        ExprResult result;
        result.place = factor->children[0]->lexeme;
        result.type = TYPE_CHAR;
        return result;
    }

    if (first_symbol == "not") {
        return make_unary("!", analyze_factor(factor->children[1]));
    }

    if (first_symbol == "minus") {
        return make_unary("-", analyze_factor(factor->children[1]));
    }

    // factor -> id factor rest
    TreeNode* id_node = factor->children[0];
    TreeNode* rest = factor->children[1];

    if (!is_epsilon_node(rest)) {
        // factor rest -> lparen arg list rparen, a call inside an expression
        return analyze_call(id_node, rest->children[1], true);
    }

    const SymbolEntry* entry = symbol_table.lookup(id_node->lexeme);
    if (entry == nullptr) {
        report_error(id_node, "undeclared identifier '" + id_node->lexeme + "'");
        return error_result();
    }

    if (entry->kind == SYMBOL_FUNCTION) {
        report_error(id_node, "'" + id_node->lexeme + "' is a function, not a variable");
        return error_result();
    }

    ExprResult result;
    result.place = entry->unique_name;
    result.type = entry->type;
    return result;
}

ExprResult Analyzer::make_binary(const std::string& op, ExprResult left, ExprResult right) {
    if (left.type == TYPE_ERROR || right.type == TYPE_ERROR) {
        return error_result();
    }

    std::string temp = make_temp();
    emit(temp + " = " + left.place + " " + op + " " + right.place);

    ExprResult result;
    result.place = temp;
    result.type = TYPE_INT;
    return result;
}

ExprResult Analyzer::make_unary(const std::string& op, ExprResult operand) {
    if (operand.type == TYPE_ERROR) {
        return error_result();
    }

    std::string temp = make_temp();
    emit(temp + " = " + op + " " + operand.place);

    ExprResult result;
    result.place = temp;
    result.type = TYPE_INT;
    return result;
}

ExprResult Analyzer::error_result() {
    ExprResult result;
    result.place = "0";
    result.type = TYPE_ERROR;
    return result;
}

std::string Analyzer::make_temp() {
    ++temporary_count;
    return "t" + std::to_string(temporary_count);
}

std::string Analyzer::make_label() {
    ++label_count;
    return "L" + std::to_string(label_count);
}

void Analyzer::emit(const std::string& line) {
    code.push_back(line);
}

void Analyzer::report_error(const TreeNode* node, const std::string& message) {
    const TreeNode* position = find_first_terminal(node);
    int line = 0;
    int column = 0;
    if (position != nullptr) {
        line = position->line;
        column = position->column;
    }

    errors.push_back("at line " + std::to_string(line) + ", col " + std::to_string(column) +
                     ": " + message);
}
