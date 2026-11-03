#pragma once
#include <map>
#include <string>
#include <vector>

#include "parser.h"
#include "symbol_table.h"

// the place and type of an analyzed expression
struct ExprResult {
    std::string place;
    ValueType type;
};

class Analyzer {
public:
    explicit Analyzer(TreeNode* root);

    // walks the tree once and fills the error and code lists
    void analyze();

    const std::vector<std::string>& get_errors() const { return errors; }
    const std::vector<std::string>& get_code() const { return code; }

private:
    void collect_function_signatures(TreeNode* program);
    void analyze_program(TreeNode* program);
    void analyze_external_decl(TreeNode* decl);
    void analyze_function(TreeNode* id_node, ValueType return_type,
                          TreeNode* param_list, TreeNode* block);

    void declare_variable(TreeNode* id_node, ValueType type);
    void declare_id_tail(ValueType type, TreeNode* id_tail);

    void analyze_block(TreeNode* block, bool open_scope);
    void analyze_stmt_list(TreeNode* stmt_list);
    void analyze_stmt(TreeNode* stmt);
    void analyze_assignment_or_call(TreeNode* id_node, TreeNode* stmt_rest);

    ExprResult analyze_call(TreeNode* id_node, TreeNode* arg_list, bool use_result);
    void collect_arg_list(TreeNode* arg_list, std::vector<ExprResult>& args);

    ExprResult analyze_expr(TreeNode* expr);
    ExprResult analyze_and_expr(TreeNode* expr);
    ExprResult analyze_rel_expr(TreeNode* expr);
    ExprResult analyze_add_expr(TreeNode* expr);
    ExprResult analyze_term(TreeNode* expr);
    ExprResult analyze_factor(TreeNode* factor);

    ExprResult make_binary(const std::string& op, ExprResult left, ExprResult right);
    ExprResult make_unary(const std::string& op, ExprResult operand);
    ExprResult error_result();

    std::string make_temp();
    std::string make_label();
    void emit(const std::string& line);
    void report_error(const TreeNode* node, const std::string& message);

    TreeNode* root;
    SymbolTable symbol_table;
    std::vector<std::string> errors;
    std::vector<std::string> code;
    std::map<std::string, SymbolEntry> function_signatures;
    int temporary_count;
    int label_count;
    ValueType current_return_type;
};
