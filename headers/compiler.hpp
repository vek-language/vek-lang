#pragma once

#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <vector>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "AST.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "bytecode.hpp"
#include "token.hpp"
#include "manifest.hpp"

using std::runtime_error;
using std::vector;
using std::unique_ptr;
using std::string;
using std::unordered_map;
using std::unordered_set;
namespace fs = std::filesystem;

class Compiler {
private:
    struct TypeInfo {
        Type kind = Type::UNKNOWN;
        std::string name; // for structs/classes, or empty for primitives

        TypeInfo() = default;
        TypeInfo(Type k, std::string n = "")
            : kind(k), name(n.empty() ? type_to_string(k) : std::move(n)) {}

        operator Type() const { return kind; }
    };

    struct LoopContext {
        vector<size_t> break_patches;
        vector<size_t> continue_patches;
        int scope_depth;
    };
    vector<LoopContext> loop_stack;

    unordered_map<string, StructDecl*> structs;
    unordered_map<string, FuncDecl*> functions;

    Type current_return_type = Type::UNKNOWN;
    unordered_map<string, Type> function_return_types;

    vector<unordered_map<string, string>> custom_types;
    vector<unordered_map<string, Type>> variable_scopes;
    vector<unordered_map<string, bool>> constant_scopes;

    unordered_map<string, vector<unique_ptr<Stmt>>> parsed_modules;
    unordered_set<string> registered_modules;
    unordered_set<string> emitted_modules;

    SourceLocation current_loc{0, 1};

    fs::path exec_dir;
    fs::path project_dir;
    fs::path src_file;

    // Scope helpers
    void push_scope() { 
        variable_scopes.push_back({}); 
        constant_scopes.push_back({});
        custom_types.push_back({});
    }
    void pop_scope() { 
        variable_scopes.pop_back(); 
        constant_scopes.pop_back();
        custom_types.pop_back();
    }

    // Type helpers
    Type token_to_type(TokenType t) {
        switch(t) {
            case TokenType::INT32:      return Type::INT32;
            case TokenType::INT64:      return Type::INT64;
            case TokenType::FLOAT32:    return Type::FLOAT32;
            case TokenType::FLOAT64:    return Type::FLOAT64;
            case TokenType::BOOL:       return Type::BOOL;
            case TokenType::CHAR:       return Type::CHAR;
            case TokenType::STR:        return Type::STR;
            case TokenType::VOID:       return Type::VOID;
            default:                    return Type::UNKNOWN;
        }
    }
    
    Type promote_numeric(Type a, Type b) {
        if (a == Type::FLOAT64 || b == Type::FLOAT64) return Type::FLOAT64;
        if (a == Type::FLOAT32 || b == Type::FLOAT32) return Type::FLOAT32;
        if (a == Type::INT64   || b == Type::INT64)   return Type::INT64;
        return Type::INT32;
    }

    Type get_var_type(const string& name) {
        for (int i = (int)variable_scopes.size() - 1; i >= 0; i--) {
            auto it = variable_scopes[i].find(name);
            if (it != variable_scopes[i].end()) return it->second;
        }
        return Type::UNKNOWN;
    }
    void set_var_type(const string& name, Type type) { 
        variable_scopes.back()[name] = type; 
    }
    bool is_constant(const string& name) {
        for (int i = (int)constant_scopes.size() - 1; i >= 0; i--) {
            auto it = constant_scopes[i].find(name);
            if (it != constant_scopes[i].end()) return it->second;
        }
        return false;
    }
    void set_const(const string& name, bool is_const) {
        constant_scopes.back()[name] = is_const;
    }

    void set_custom_type(const string& name, const string& type_name) {
        if (!custom_types.empty()) {
            custom_types.back()[name] = type_name;
        }
    }

    string get_custom_type(const string& name) {
        for (int i = static_cast<int>(custom_types.size()) - 1; i >= 0; --i) {
            auto it = custom_types[i].find(name);
            if (it != custom_types[i].end()) return it->second;
        }
        return "";
    }

    void emit_compound_math(TokenType op, Bytecode& bytecode) {
        switch (op) {
            case TokenType::PLUS:  bytecode.emit(OpCode::OP_ADD); break;
            case TokenType::MINUS: bytecode.emit(OpCode::OP_SUBTRACT); break;
            case TokenType::STAR:  bytecode.emit(OpCode::OP_MULTIPLY); break;
            case TokenType::SLASH: bytecode.emit(OpCode::OP_DIVIDE); break;
            default: report_error(current_loc, "Unknown compound assignment operator");
        }
    }

    bool is_numeric(Type t) { return t == Type::INT32 || t == Type::INT64 || t == Type::FLOAT32 || t == Type::FLOAT64; }
    bool is_string_or_char(Type t) { return t == Type::STR || t == Type::CHAR; }
    bool is_assignable(Type target, Type source) { return target == source || (is_numeric(target) && is_numeric(source)); }

    void patch_jump(Bytecode& bytecode, size_t jump_index, size_t target) {
        bytecode.code[jump_index] = (target >> 8) & 0xFF;
        bytecode.code[jump_index + 1] = target & 0xFF;
    }

    // Emit: [opcode][u16 constant index]
    void emit_const_idx(Bytecode& bytecode, OpCode op, uint16_t idx) {
        bytecode.write(static_cast<uint8_t>(op));
        bytecode.emit_u16(idx);
    }

    // Emit: [opcode][u16 constant index][u8 extra]
    void emit_const_idx_u8(Bytecode& bytecode, OpCode op, uint16_t idx, uint8_t extra) {
        bytecode.write(static_cast<uint8_t>(op));
        bytecode.emit_u16(idx);
        bytecode.write(extra);
    }

    // Module loading
    fs::path resolve_load_path(LoadStmt* stmt, const fs::path& current_file) {
        fs::path resolved;

        if (stmt->is_stdlib) {
            // Check project packages
            fs::path packages_root = project_dir / ".vek" / "packages";
            fs::path pkg_dir = packages_root / stmt->filepath;
            Manifest pkg_manifest;

            if (fs::exists(pkg_dir) && fs::is_directory(pkg_dir)) {
                load_manifest(pkg_dir / "vek.toml", pkg_manifest);
            } else if (fs::exists(packages_root)) {
                for (const auto& dir_entry : fs::directory_iterator(packages_root)) {
                    if (!dir_entry.is_directory()) continue;
                    Manifest candidate;
                    load_manifest(dir_entry.path() / "vek.toml", candidate);
                    if (candidate.metadata.name == stmt->filepath) {
                        pkg_dir = dir_entry.path();
                        pkg_manifest = std::move(candidate);
                        break;
                    }
                }
            }

            if (fs::exists(pkg_dir) && fs::is_directory(pkg_dir)) {
                std::string entry = pkg_manifest.entry.empty() ? "init.vek" : pkg_manifest.entry;
                resolved = fs::weakly_canonical(pkg_dir / entry);

                if (!fs::exists(resolved)) {
                    report_error(
                        current_loc,
                        "Package '" + stmt->filepath + "' entry file not found: '" + entry + "'"
                    );
                }
                return resolved;
            }

            // Fallback to built-in stdlib
            fs::path base = exec_dir.empty() ? fs::current_path() : exec_dir;
            resolved = fs::weakly_canonical(base / "libs" / (stmt->filepath + ".vek"));

            if (!fs::exists(resolved)) {
                report_error(
                    current_loc,
                    "Module '" + stmt->filepath + "' not found in project packages or standard library."
                );
            }
            return resolved;
        }

        // Resolve relative to the file doing the "load" (quoted)
        fs::path parent = current_file.has_parent_path()
            ? current_file.parent_path()
            : fs::current_path();
        resolved = fs::weakly_canonical(parent / (stmt->filepath + ".vek"));

        if (!fs::exists(resolved)) {
            report_error(
                current_loc,
                "File not found: '" + stmt->filepath + ".vek'"
            );
        }
        return resolved;
    }

    void parse_and_register_module(LoadStmt* stmt, const fs::path& current_file) {
        fs::path full_path = resolve_load_path(stmt, current_file);
        string key = full_path.string();

        if (registered_modules.count(key)) return;
        registered_modules.insert(key);

        std::ifstream file(full_path);
        if (!file.is_open()) {
            report_error(current_loc, "Could not find module: " + stmt->filepath + " (looked at: " + key + ")");
        }
        std::stringstream buffer;
        buffer << file.rdbuf();

        Lexer lexer(buffer.str());
        auto tokens = lexer.lex();
        Parser parser(tokens);
        auto program = parser.parse_program();

        for (const auto& s : program) {
            if (auto* load = dynamic_cast<LoadStmt*>(s.get())) {
                parse_and_register_module(load, full_path);
            }
        }

        for (const auto& s : program) {
            if (auto* func = dynamic_cast<FuncDecl*>(s.get())) {
                if (func->name == "main") {
                    report_error(current_loc, "Modules cannot define 'main'.");
                }
                functions[func->name] = func;
                function_return_types[func->name] = token_to_type(func->return_type);
            }
            if (auto* strct = dynamic_cast<StructDecl*>(s.get())) {
                structs[strct->name] = strct;
            }
        }

        parsed_modules[key] = std::move(program);
    }

    void emit_module_globals(LoadStmt* stmt, const fs::path& current_file, Bytecode& bytecode) {
        fs::path full_path = resolve_load_path(stmt, current_file);
        string key = full_path.string();

        parse_and_register_module(stmt, current_file);

        if (emitted_modules.count(key)) return;
        emitted_modules.insert(key);

        const auto& program = parsed_modules[key];

        for (const auto& s : program) {
            if (auto* load = dynamic_cast<LoadStmt*>(s.get())) {
                emit_module_globals(load, full_path, bytecode);
            }
        }

        for (const auto& s : program) {
            if (dynamic_cast<FuncDecl*>(s.get()) || dynamic_cast<LoadStmt*>(s.get())) continue;
            compile_stmt(s.get(), bytecode);
        }
    }

    bool ends_with_return(Stmt* stmt) {
        if (dynamic_cast<ReturnStmt*>(stmt)) return true;
        if (auto* block = dynamic_cast<BlockStmt*>(stmt)) {
            if (block->statements.empty()) return false;
            return ends_with_return(block->statements.back().get());
        }
        if (auto* if_stmt = dynamic_cast<IfStmt*>(stmt)) {
            // Both branches must return.
            return if_stmt->else_branch
                && ends_with_return(if_stmt->then_branch.get())
                && ends_with_return(if_stmt->else_branch.get());
        }
        return false;
    }

    void emit_function_body(FuncDecl* func, Bytecode& bytecode) {
        bytecode.function_addresses[func->name] = bytecode.code.size();
        
        push_scope();
        for (const auto& param : func->params) {
            uint16_t name_idx = bytecode.add_const(param.name);
            emit_const_idx(bytecode, param.is_ref ? OpCode::OP_DEFINE_REF_VAR : OpCode::OP_DEFINE_VAR, name_idx);
            set_var_type(param.name, token_to_type(param.type));
        }
        
        Type saved_return_type = current_return_type;
        current_return_type = token_to_type(func->return_type);

        for (const auto& body_stmt : func->body) {
            compile_stmt(body_stmt.get(), bytecode);
        }

        current_return_type = saved_return_type;
        pop_scope();

        // Guarantee a return so control never falls through into the next function.
        // If the body does not provably end in 'ret', emit an implicit default return.
        if (func->body.empty() || !ends_with_return(func->body.back().get())) {
            if (token_to_type(func->return_type) != Type::VOID
                && token_to_type(func->return_type) != Type::UNKNOWN) {
                report_error(current_loc, "Function '" + func->name + "' must return a value of type "
                    + type_to_string(token_to_type(func->return_type)));
            }
            uint16_t idx = bytecode.add_const((int64_t)0);
            emit_const_idx(bytecode, OpCode::OP_CONSTANT, idx);
            bytecode.emit(OpCode::OP_RETURN);
        }
    }

    // Control flow
    void compile_if(IfStmt* stmt, Bytecode& bytecode) {
        compile_expr(stmt->condition.get(), bytecode, Type::BOOL);
            
        bytecode.emit(OpCode::OP_JUMP_IF_FALSE, 0, 0);
        size_t jump_if_false = bytecode.code.size() - 2;
        
        compile_stmt(stmt->then_branch.get(), bytecode);
        
        if (stmt->else_branch) {
            bytecode.emit(OpCode::OP_JUMP, 0, 0);
            size_t jump_over_else = bytecode.code.size() - 2;
            
            patch_jump(bytecode, jump_if_false, bytecode.code.size());
            compile_stmt(stmt->else_branch.get(), bytecode);
            patch_jump(bytecode, jump_over_else, bytecode.code.size());
        } else {
            patch_jump(bytecode, jump_if_false, bytecode.code.size());
        }
    }

    void compile_while(WhileStmt* stmt, Bytecode& bytecode) {
        size_t loop_start = bytecode.code.size();
        loop_stack.push_back({ {}, {}, (int)variable_scopes.size() });

        compile_expr(stmt->condition.get(), bytecode, Type::BOOL);
        bytecode.emit(OpCode::OP_JUMP_IF_FALSE, 0, 0);
        size_t exit_jump = bytecode.code.size() - 2;

        compile_stmt(stmt->body.get(), bytecode);

        // "continue"
        for (size_t cont_jump : loop_stack.back().continue_patches) {
            patch_jump(bytecode, cont_jump, loop_start);
        }

        bytecode.emit(OpCode::OP_JUMP, (loop_start >> 8) & 0xFF, loop_start & 0xFF);

        // "break"
        size_t loop_end = bytecode.code.size();
        patch_jump(bytecode, exit_jump, loop_end);
        for (size_t break_jump : loop_stack.back().break_patches) {
            patch_jump(bytecode, break_jump, loop_end);
        }

        loop_stack.pop_back();
    }

    void compile_for(ForStmt* stmt, Bytecode& bytecode) {
        bytecode.emit(OpCode::OP_ENTER_SCOPE);
        push_scope();

        if (stmt->initializer) {
            compile_stmt(stmt->initializer.get(), bytecode);
        }

        size_t loop_start = bytecode.code.size();

        // Push loop context after scope created
        // "break" jumps to OP_EXIT_SCOPE
        loop_stack.push_back({ {}, {}, (int)variable_scopes.size() });

        size_t exit_jump = 0;
        if (stmt->condition) {
            compile_expr(stmt->condition.get(), bytecode, Type::BOOL);
            bytecode.emit(OpCode::OP_JUMP_IF_FALSE, 0, 0);
            exit_jump = bytecode.code.size() - 2;
        }

        compile_stmt(stmt->body.get(), bytecode);

        // "continue" there, so stmt->increment runs
        size_t increment_start = bytecode.code.size();
        for (size_t cont_jump : loop_stack.back().continue_patches) {
            patch_jump(bytecode, cont_jump, increment_start);
        }

        if (stmt->increment) {
            compile_expr(stmt->increment.get(), bytecode, Type::UNKNOWN);
            if (!dynamic_cast<AssignExpr*>(stmt->increment.get())) {
                bytecode.emit(OpCode::OP_POP);
            }
        }

        bytecode.emit(OpCode::OP_JUMP, (loop_start >> 8) & 0xFF, loop_start & 0xFF);

        // "break"
        size_t loop_end = bytecode.code.size();
        if (stmt->condition) {
            patch_jump(bytecode, exit_jump, loop_end);
        }
        for (size_t break_jump : loop_stack.back().break_patches) {
            patch_jump(bytecode, break_jump, loop_end);
        }

        loop_stack.pop_back();

        bytecode.emit(OpCode::OP_EXIT_SCOPE);
        pop_scope();
    }

    void compile_break(BreakStmt* stmt, Bytecode& bytecode) {
        if (loop_stack.empty()) {
            report_error(current_loc, "'break' used outside of a loop.");
        }
        int scopes_to_exit = (int)variable_scopes.size() - loop_stack.back().scope_depth;
        for (int i = 0; i < scopes_to_exit; i++) {
            bytecode.emit(OpCode::OP_EXIT_SCOPE);
        }
        bytecode.emit(OpCode::OP_JUMP, 0, 0);
        loop_stack.back().break_patches.push_back(bytecode.code.size() - 2);
    }

    void compile_continue(ContinueStmt* stmt, Bytecode& bytecode) {
        if (loop_stack.empty()) {
            report_error(current_loc, "'continue' used outside of a loop.");
        }
        int scopes_to_exit = (int)variable_scopes.size() - loop_stack.back().scope_depth;
        for (int i = 0; i < scopes_to_exit; i++) {
            bytecode.emit(OpCode::OP_EXIT_SCOPE);
        }
        bytecode.emit(OpCode::OP_JUMP, 0, 0);
        loop_stack.back().continue_patches.push_back(bytecode.code.size() - 2);
    }

    // Statements
    void compile_var_decl(VarDecl* var_decl, Bytecode& bytecode) {
        Type declared_type = token_to_type(var_decl->type);

        // Handle struct/class variable declarations
        if (var_decl->type == TokenType::IDENTIFIER) {
            if (!structs.count(var_decl->type_name)) {
                report_error(current_loc, "Unknown struct '" + var_decl->type_name + "'");
            }
            declared_type = Type::STRUCT;
            set_custom_type(var_decl->name, var_decl->type_name);

            if (!var_decl->is_array) {
                if (auto* lit = dynamic_cast<StructLiteral*>(var_decl->initializer.get())) {
                    if (lit->struct_name.empty()) {
                        lit->struct_name = var_decl->type_name;
                    }
                } else if (auto* empty_arr = dynamic_cast<ArrayLiteral*>(var_decl->initializer.get())) {
                    if (empty_arr->elements.empty()) {
                        var_decl->initializer = make_unique<StructLiteral>(
                            var_decl->type_name, vector<pair<string, unique_ptr<Expr>>>{}
                        );
                    }
                }
            }
        }

        set_var_type(var_decl->name, var_decl->is_array ? Type::ARRAY : declared_type);
        set_const(var_decl->name, var_decl->is_const);

        if (var_decl->is_array) {
            bool handled = false;
            
            if (auto* arr_lit = dynamic_cast<ArrayLiteral*>(var_decl->initializer.get())) {
                int64_t size = arr_lit->elements.size();
                uint16_t idx = bytecode.add_const(size);
                emit_const_idx(bytecode, OpCode::OP_CONSTANT, idx);
                
                bytecode.emit(OpCode::OP_NEW_ARRAY);
                uint16_t name_index = bytecode.add_const(var_decl->name);
                emit_const_idx(bytecode, OpCode::OP_DEFINE_VAR, name_index);

                for (size_t i = 0; i < arr_lit->elements.size(); ++i) {
                    if (var_decl->type == TokenType::IDENTIFIER) {
                        if (auto* lit = dynamic_cast<StructLiteral*>(arr_lit->elements[i].get())) {
                            if (lit->struct_name.empty()) {
                                lit->struct_name = var_decl->type_name;
                            }
                        }
                    }

                    uint16_t var_idx = bytecode.add_const(var_decl->name);
                    emit_const_idx(bytecode, OpCode::OP_LOAD, var_idx);

                    uint16_t idx_val = bytecode.add_const((int64_t)i);
                    emit_const_idx(bytecode, OpCode::OP_CONSTANT, idx_val);

                    Type elem_type = compile_expr(arr_lit->elements[i].get(), bytecode, declared_type);
                    if (declared_type != Type::UNKNOWN && !is_assignable(declared_type, elem_type)) {
                        report_error(current_loc,
                            "Array element type mismatch: expected " + type_to_string(declared_type) +
                            ", got " + type_to_string(elem_type));
                    }
                    bytecode.emit(OpCode::OP_SET_INDEX);
                }
                handled = true;
            } 
            else if (var_decl->array_size) {
                compile_expr(var_decl->array_size.get(), bytecode, Type::INT64);
                bytecode.emit(OpCode::OP_NEW_ARRAY);
                uint16_t name_index = bytecode.add_const(var_decl->name);
                emit_const_idx(bytecode, OpCode::OP_DEFINE_VAR, name_index);
                handled = true;
            }
            else if (var_decl->initializer) {
                Type init_type = compile_expr(var_decl->initializer.get(), bytecode, Type::ARRAY);
                if (init_type != Type::ARRAY && init_type != Type::UNKNOWN) {
                    report_error(current_loc, "Array initializer must be an array, got "
                        + type_to_string(init_type));
                }
                uint16_t name_index = bytecode.add_const(var_decl->name);
                emit_const_idx(bytecode, OpCode::OP_DEFINE_VAR, name_index);
                handled = true;
            }

            if (!handled) {
                report_error(current_loc, "Array size error. Provide [N] or an initializer.");
            }
        } else {
            Type init_type = compile_expr(var_decl->initializer.get(), bytecode, declared_type);
            if (declared_type != Type::UNKNOWN && !is_assignable(declared_type, init_type)) {
                report_error(current_loc,
                    "Cannot initialize " + type_to_string(declared_type) +
                    " with " + type_to_string(init_type));
            }
            uint16_t name_index = bytecode.add_const(var_decl->name);
            emit_const_idx(bytecode, OpCode::OP_DEFINE_VAR, name_index);
        }
    }

    void compile_print(PrintStmt* print_stmt, Bytecode& bytecode) {
        compile_expr(print_stmt->expression.get(), bytecode, Type::UNKNOWN);
        bytecode.emit(OpCode::OP_PRINT);
    }

    void compile_input(InputStmt* stmt, Bytecode& bytecode) {
        if (auto* var = dynamic_cast<VariableExpr*>(stmt->expression.get())) {
            Type var_type = get_var_type(var->name);
            if (var_type == Type::UNKNOWN) {
                report_error(current_loc, "Undefined variable in input(): " + var->name);
            }
            if (is_constant(var->name)) {
                report_error(current_loc, "Cannot read input into constant variable: " + var->name);
            }

            bytecode.emit(OpCode::OP_INPUT, static_cast<uint8_t>(var_type));
            uint16_t name_idx = bytecode.add_const(var->name);
            emit_const_idx(bytecode, OpCode::OP_SAVE, name_idx);
        }
    }

    void compile_return(ReturnStmt* ret_stmt, Bytecode& bytecode) {
        if (ret_stmt->value) {
            Type val_type = compile_expr(ret_stmt->value.get(), bytecode, current_return_type);
            if (current_return_type != Type::UNKNOWN && current_return_type != Type::VOID
                && !is_assignable(current_return_type, val_type)) {
                report_error(current_loc,
                    "Return type mismatch: expected " + type_to_string(current_return_type) +
                    ", got " + type_to_string(val_type));
            }
        } else {
            if (current_return_type != Type::UNKNOWN && current_return_type != Type::VOID) {
                report_error(current_loc, "Function must return a value of type "
                    + type_to_string(current_return_type));
            }
            uint16_t idx = bytecode.add_const((int64_t)0);
            emit_const_idx(bytecode, OpCode::OP_CONSTANT, idx);
        }
        bytecode.emit(OpCode::OP_RETURN);
    }

    // Dispatcher
    void compile_stmt(Stmt* stmt, Bytecode& bytecode) {
        if (stmt->location.line != 0) current_loc = stmt->location;

        if (auto* block = dynamic_cast<BlockStmt*>(stmt)) {
            bytecode.emit(OpCode::OP_ENTER_SCOPE);
            push_scope();
            for (const auto& s : block->statements) {
                compile_stmt(s.get(), bytecode);
            }
            bytecode.emit(OpCode::OP_EXIT_SCOPE);
            pop_scope();
            return;
        }

        if (auto* struct_decl = dynamic_cast<StructDecl*>(stmt)) {
            return; 
        }
        if (auto* load_stmt = dynamic_cast<LoadStmt*>(stmt)) { 
            emit_module_globals(load_stmt, src_file, bytecode); 
            return; 
        }
        if (auto* if_stmt = dynamic_cast<IfStmt*>(stmt)) { compile_if(if_stmt, bytecode); return; }
        if (auto* while_stmt = dynamic_cast<WhileStmt*>(stmt)) { compile_while(while_stmt, bytecode); return; }
        if (auto* for_stmt = dynamic_cast<ForStmt*>(stmt)) { compile_for(for_stmt, bytecode); return; }
        if (auto* break_stmt = dynamic_cast<BreakStmt*>(stmt)) { compile_break(break_stmt, bytecode); return; }
        if (auto* continue_stmt = dynamic_cast<ContinueStmt*>(stmt)) { compile_continue(continue_stmt, bytecode); return; }

        if (auto* var_decl = dynamic_cast<VarDecl*>(stmt)) { compile_var_decl(var_decl, bytecode); return; }
        if (auto* print_stmt = dynamic_cast<PrintStmt*>(stmt)) { compile_print(print_stmt, bytecode); return; }
        if (auto* input_stmt = dynamic_cast<InputStmt*>(stmt)) { compile_input(input_stmt, bytecode); return; }
        if (auto* ret_stmt = dynamic_cast<ReturnStmt*>(stmt)) { compile_return(ret_stmt, bytecode); return; }

        if (auto* func = dynamic_cast<FuncDecl*>(stmt)) { emit_function_body(func, bytecode); return; }

        if (auto* expr_stmt = dynamic_cast<ExprStmt*>(stmt)) {
            compile_expr(expr_stmt->expression.get(), bytecode, Type::UNKNOWN);
            if (!dynamic_cast<AssignExpr*>(expr_stmt->expression.get())) {
                bytecode.emit(OpCode::OP_POP);
            }
            return;
        }
        
        report_error(current_loc, "Unknown statement type");
    }

    // Expressions
    Type compile_expr(Expr* expr, Bytecode& bytecode, Type expected_type = Type::UNKNOWN) {
        if (expr->location.line != 0) current_loc = expr->location;

        if (auto* i64 = dynamic_cast<Int64Literal*>(expr)) {
            if (expected_type == Type::INT32) {
                uint16_t idx = bytecode.add_const(static_cast<int32_t>(i64->value));
                emit_const_idx(bytecode, OpCode::OP_CONSTANT, idx);
                return Type::INT32;
            }

            uint16_t idx = bytecode.add_const(static_cast<int64_t>(i64->value));
            emit_const_idx(bytecode, OpCode::OP_CONSTANT, idx);
            return Type::INT64;
        }
        if (auto* f64 = dynamic_cast<Float64Literal*>(expr)) {
            uint16_t idx = bytecode.add_const(static_cast<double>(f64->value));
            emit_const_idx(bytecode, OpCode::OP_CONSTANT, idx);
            return Type::FLOAT64;
        }
        if (auto* f32 = dynamic_cast<Float32Literal*>(expr)) {
            uint16_t idx = bytecode.add_const(static_cast<float>(f32->value));
            emit_const_idx(bytecode, OpCode::OP_CONSTANT, idx);
            return Type::FLOAT32;
        }
        if (auto* b = dynamic_cast<BoolLiteral*>(expr)) {
            uint16_t idx = bytecode.add_const(b->value);
            emit_const_idx(bytecode, OpCode::OP_CONSTANT, idx);
            return Type::BOOL;
        }
        if (auto* s = dynamic_cast<StringLiteral*>(expr)) {
            uint16_t idx = bytecode.add_const(s->value);
            emit_const_idx(bytecode, OpCode::OP_CONSTANT, idx);
            return Type::STR;
        }
        if (auto* c = dynamic_cast<CharLiteral*>(expr)) {
            uint16_t idx = bytecode.add_const(c->value);
            emit_const_idx(bytecode, OpCode::OP_CONSTANT, idx);
            return Type::CHAR;
        }
        
        if (auto* t_expr = dynamic_cast<TypeofExpr*>(expr)) {
            Type t = compile_expr(t_expr->operand.get(), bytecode, Type::UNKNOWN);
            bytecode.emit(OpCode::OP_POP);

            string result = type_to_string(t);
            if (auto* var = dynamic_cast<VariableExpr*>(t_expr->operand.get())) {
                string custom = get_custom_type(var->name);
                if (!custom.empty()) result = custom;
            }

            uint16_t idx = bytecode.add_const(result);
            emit_const_idx(bytecode, OpCode::OP_CONSTANT, idx);
            return Type::STR;
        }

        if (auto* var = dynamic_cast<VariableExpr*>(expr)) {
            uint16_t idx = bytecode.add_const(var->name);
            emit_const_idx(bytecode, OpCode::OP_LOAD, idx);
            return get_var_type(var->name);
        }

        if (auto* field = dynamic_cast<FieldAccessExpr*>(expr)) {
            compile_expr(field->object.get(), bytecode, Type::UNKNOWN);
            uint16_t field_idx = bytecode.add_const(field->field);
            emit_const_idx(bytecode, OpCode::OP_GET_MEMBER, field_idx);
            return Type::UNKNOWN;
        }
        if (auto* lit = dynamic_cast<StructLiteral*>(expr)) {
            if (!structs.count(lit->struct_name)) {
                report_error(current_loc, "Unknown struct '" + lit->struct_name + "'");
            }
            
            uint16_t name_idx = bytecode.add_const(lit->struct_name);
            uint8_t field_count = static_cast<uint8_t>(lit->initializers.size());

            for (const auto& [field_name, field_expr] : lit->initializers) {
                uint16_t field_idx = bytecode.add_const(field_name);
                emit_const_idx(bytecode, OpCode::OP_CONSTANT, field_idx);
                compile_expr(field_expr.get(), bytecode, Type::UNKNOWN);
            }

            emit_const_idx_u8(bytecode, OpCode::OP_NEW_STRUCT, name_idx, field_count);
            return TypeInfo(Type::STRUCT, lit->struct_name);
        }

        if (auto* assign_expr = dynamic_cast<AssignExpr*>(expr)) {
            // Variable
            if (auto* var = dynamic_cast<VariableExpr*>(assign_expr->target.get())) {
                Type var_type = get_var_type(var->name);
                if (var_type == Type::UNKNOWN) {
                    report_error(current_loc, "Undefined variable '" + var->name + "'");
                }
                if (is_constant(var->name)) {
                    report_error(current_loc, "Cannot reassign constant variable '" + var->name + "'");
                }

                uint16_t var_idx = bytecode.add_const(var->name);

                if (assign_expr->op == TokenType::EQUALS) {
                    Type val_type = compile_expr(assign_expr->value.get(), bytecode, var_type);
                    if (!is_assignable(var_type, val_type)) {
                        report_error(current_loc, "Cannot assign " + type_to_string(val_type) + " to " + type_to_string(var_type));
                    }
                    emit_const_idx(bytecode, OpCode::OP_SAVE, var_idx);
                } else {
                    emit_const_idx(bytecode, OpCode::OP_LOAD, var_idx);
                    compile_expr(assign_expr->value.get(), bytecode, var_type);
                    switch (assign_expr->op) {
                        case TokenType::PLUS:  bytecode.emit(OpCode::OP_ADD);       break;
                        case TokenType::MINUS: bytecode.emit(OpCode::OP_SUBTRACT);  break;
                        case TokenType::STAR:  bytecode.emit(OpCode::OP_MULTIPLY);  break;
                        case TokenType::SLASH: bytecode.emit(OpCode::OP_DIVIDE);    break;
                        default: report_error(current_loc, "Invalid assignment operator");
                    }
                    emit_const_idx(bytecode, OpCode::OP_SAVE, var_idx);
                }
                return Type::UNKNOWN;
            }

            // Array element
            if (auto* arr = dynamic_cast<ArrayAccessExpr*>(assign_expr->target.get())) {
                if (assign_expr->op != TokenType::EQUALS) {
                    report_error(current_loc, "Compound assignment on array elements is not supported");
                }
                compile_expr(arr->array.get(), bytecode, Type::UNKNOWN);
                compile_expr(arr->index.get(), bytecode, Type::INT64);
                compile_expr(assign_expr->value.get(), bytecode, Type::UNKNOWN);
                bytecode.emit(OpCode::OP_SET_INDEX);
                return Type::UNKNOWN;
            }

            // Struct/Class member
            if (auto* field = dynamic_cast<FieldAccessExpr*>(assign_expr->target.get())) {
                if (assign_expr->op != TokenType::EQUALS) {
                    report_error(current_loc, "Compound assignment on struct fields is not supported");
                }
                compile_expr(field->object.get(), bytecode, Type::UNKNOWN);
                compile_expr(assign_expr->value.get(), bytecode, Type::UNKNOWN);
                uint16_t field_idx = bytecode.add_const(field->field);
                emit_const_idx(bytecode, OpCode::OP_SET_MEMBER, field_idx);
                return Type::UNKNOWN;
            }

            report_error(current_loc, "Invalid assignment target");
        }

        if (auto* arr_access = dynamic_cast<ArrayAccessExpr*>(expr)) {
            compile_expr(arr_access->array.get(), bytecode);
            compile_expr(arr_access->index.get(), bytecode, Type::INT64);
            bytecode.emit(OpCode::OP_GET_INDEX);
            return Type::UNKNOWN;
        }

        if (auto* method_call = dynamic_cast<MethodCallExpr*>(expr)) {
            compile_expr(method_call->object.get(), bytecode, Type::ARRAY);
            for (const auto& arg : method_call->arguments) {
                compile_expr(arg.get(), bytecode, Type::UNKNOWN);
            }
            if (method_call->method == "push") {
                if (method_call->arguments.size() != 1) report_error(current_loc, "push expects 1 argument");
                bytecode.emit(OpCode::OP_ARRAY_PUSH);
            } 
            else if (method_call->method == "pop") {
                if (!method_call->arguments.empty()) report_error(current_loc, "pop expects 0 arguments");
                bytecode.emit(OpCode::OP_ARRAY_POP);
            } 
            else {
                report_error(current_loc, "Unknown array method: " + method_call->method);
            }
            return Type::UNKNOWN;
        }

        if (auto* call = dynamic_cast<CallExpr*>(expr)) {
             auto nit = bytecode.native_index.find(call->name);
            if (nit != bytecode.native_index.end()) {
                const NativeSymbol& sym = bytecode.natives[nit->second];
                int argc = static_cast<int>(call->arguments.size());

                if (argc < sym.min_arity || argc > sym.max_arity)
                    report_error(current_loc, sym.name + ": expected " +
                        (sym.min_arity == sym.max_arity
                            ? std::to_string(sym.min_arity)
                            : std::to_string(sym.min_arity) + ".." + std::to_string(sym.max_arity)) +
                        " args, got " + std::to_string(argc));

                for (int i = argc - 1; i >= 0; --i)
                    compile_expr(call->arguments[i].get(), bytecode, Type::UNKNOWN);

                uint16_t id = nit->second;
                bytecode.emit(OpCode::OP_NATIVE_CALL,
                            (id >> 8) & 0xFF, id & 0xFF, static_cast<uint8_t>(argc));
                return sym.ret_type;
            }

            FuncDecl* func = nullptr;
            if (functions.count(call->name)) func = functions[call->name];

            if (func && func->params.size() != call->arguments.size()) {
                report_error(current_loc, "Function '" + call->name + "' expects " +
                    std::to_string(func->params.size()) + " args, got " +
                    std::to_string(call->arguments.size()));
            }

            for (size_t i = 0; i < call->arguments.size(); i++) {
                size_t idx = call->arguments.size() - 1 - i;   // reverse iteration
                auto& arg = call->arguments[idx];
                bool is_ref = func && func->params[idx].is_ref;

                if (is_ref) {
                    auto* v = dynamic_cast<VariableExpr*>(arg.get());
                    if (!v) report_error(current_loc, "Ref argument must be a variable");
                    uint16_t name_idx = bytecode.add_const(v->name);
                    emit_const_idx(bytecode, OpCode::OP_LOAD_REF, name_idx);
                } else {
                    Type arg_type = compile_expr(arg.get(), bytecode, Type::UNKNOWN);
                    if (func) {
                        Type param_type = token_to_type(func->params[idx].type);
                        if (param_type != Type::UNKNOWN && !is_assignable(param_type, arg_type)) {
                            report_error(current_loc, "Argument " + std::to_string(idx + 1) +
                                " of '" + call->name + "': expected " + type_to_string(param_type) +
                                ", got " + type_to_string(arg_type));
                        }
                    }
                }
            }

            uint16_t name_idx = bytecode.add_const(call->name);
            emit_const_idx_u8(bytecode, OpCode::OP_CALL, name_idx, static_cast<uint8_t>(call->arguments.size()));
            return function_return_types.count(call->name)
                ? function_return_types[call->name] : Type::UNKNOWN;
        }

        if (auto* unary = dynamic_cast<UnaryExpr*>(expr)) {
            compile_expr(unary->right.get(), bytecode, Type::UNKNOWN);
            if (unary->op == TokenType::NOT) {
                bytecode.emit(OpCode::OP_NOT);
            }
            return Type::BOOL;
        }

        if (auto* cast = dynamic_cast<CastExpr*>(expr)) {
            compile_expr(cast->expr.get(), bytecode, Type::UNKNOWN);
            bytecode.emit(OpCode::OP_CAST, static_cast<uint8_t>(cast->target_type));
            return token_to_type(cast->target_type);
        }

        if (auto* bin = dynamic_cast<BinaryExpr*>(expr)) {
            // Short-circuit logical operators
            if (bin->op == TokenType::AND) {
                compile_expr(bin->left.get(), bytecode, Type::UNKNOWN);
                bytecode.emit(OpCode::OP_DUPLICATE);
                bytecode.emit(OpCode::OP_JUMP_IF_FALSE, 0, 0);
                size_t jump_if_false = bytecode.code.size() - 2;

                bytecode.emit(OpCode::OP_POP);
                compile_expr(bin->right.get(), bytecode, Type::UNKNOWN);

                patch_jump(bytecode, jump_if_false, bytecode.code.size());
                return Type::BOOL;
            }
            if (bin->op == TokenType::OR) {
                compile_expr(bin->left.get(), bytecode, Type::UNKNOWN);
                bytecode.emit(OpCode::OP_DUPLICATE);
                bytecode.emit(OpCode::OP_JUMP_IF_TRUE, 0, 0);
                size_t jump_if_true = bytecode.code.size() - 2;

                bytecode.emit(OpCode::OP_POP);
                compile_expr(bin->right.get(), bytecode, Type::UNKNOWN);

                patch_jump(bytecode, jump_if_true, bytecode.code.size());
                return Type::BOOL;
            }
            
            Type left_type = compile_expr(bin->left.get(), bytecode, Type::UNKNOWN);
            Type right_type = compile_expr(bin->right.get(), bytecode, Type::UNKNOWN);

            Type result_type = Type::UNKNOWN;

            if (bin->op == TokenType::PLUS) {
                if (is_string_or_char(left_type) && is_string_or_char(right_type)) {
                    result_type = Type::STR;
                } else if (is_numeric(left_type) && is_numeric(right_type)) {
                    result_type = promote_numeric(left_type, right_type);
                } else {
                    report_error(current_loc, "Cannot add " + type_to_string(left_type) + " and " + type_to_string(right_type));
                }
            }
            else if (bin->op == TokenType::EQUALS_EQUALS || bin->op == TokenType::NOT_EQUALS) {
                bool is_valid_number = is_numeric(left_type) && is_numeric(right_type);
                bool is_same_primitive = (left_type == right_type) &&
                    (left_type == Type::STR || left_type == Type::CHAR || left_type == Type::BOOL);

                if (!is_valid_number && !is_same_primitive) {
                    report_error(current_loc, "Cannot compare " + type_to_string(left_type) + " and " + type_to_string(right_type));
                }
                result_type = Type::BOOL;
            }
            else if (bin->op == TokenType::LESS_THAN || bin->op == TokenType::LESS_EQUAL ||
                    bin->op == TokenType::GREATER_THAN || bin->op == TokenType::GREATER_EQUAL) {
                if (!is_numeric(left_type) || !is_numeric(right_type)) {
                    report_error(current_loc, "Relational comparisons require numbers, got " + type_to_string(left_type) + " and " + type_to_string(right_type));
                }
                result_type = Type::BOOL;
            }
            else {
                // -, *, /
                if (!is_numeric(left_type) || !is_numeric(right_type)) {
                    report_error(current_loc, "Math operations require numbers, got " + type_to_string(left_type) + " and " + type_to_string(right_type));
                }
                result_type = promote_numeric(left_type, right_type);
            }

            switch (bin->op) {
                case TokenType::PLUS:           bytecode.emit(OpCode::OP_ADD); break;
                case TokenType::MINUS:          bytecode.emit(OpCode::OP_SUBTRACT); break;
                case TokenType::STAR:           bytecode.emit(OpCode::OP_MULTIPLY); break;
                case TokenType::SLASH:          bytecode.emit(OpCode::OP_DIVIDE); break;
                case TokenType::GREATER_THAN:   bytecode.emit(OpCode::OP_GREATER_THAN); break;
                case TokenType::LESS_THAN:      bytecode.emit(OpCode::OP_LESS_THAN); break;
                case TokenType::GREATER_EQUAL:  bytecode.emit(OpCode::OP_GREATER_EQUAL); break;
                case TokenType::LESS_EQUAL:     bytecode.emit(OpCode::OP_LESS_EQUAL); break;
                case TokenType::EQUALS_EQUALS:  bytecode.emit(OpCode::OP_EQUAL); break;
                case TokenType::NOT_EQUALS: 
                    bytecode.emit(OpCode::OP_EQUAL); 
                    bytecode.emit(OpCode::OP_NOT); 
                    break;

                default: report_error(current_loc, "Unknown binary operator");
            }
            return result_type;
        }
        report_error(current_loc, "Unknown expression type");
    }

public:
    void compile(const vector<unique_ptr<Stmt>>& program,
            Bytecode& bytecode,
            const fs::path& entry_file,
            const fs::path& executable_dir = {}) 
    {    
        src_file = fs::weakly_canonical(entry_file);
        exec_dir = executable_dir.empty() ? fs::path{} : fs::weakly_canonical(executable_dir);
        project_dir = fs::weakly_canonical(src_file).parent_path();
        
        fs::path probe = project_dir;
        while (!probe.empty() && probe != probe.root_path()) {
            if (fs::exists(probe / "vek.toml") || fs::exists(probe / ".vek")) {
                project_dir = probe;
                break;
            }
            probe = probe.parent_path();
        }

        Manifest manifest;
        load_manifest(src_file, manifest);
        bytecode.metadata = manifest.metadata;
        push_scope();

        // Register functions, modules and structs
        for (const auto& stmt : program) {
            if (auto* func = dynamic_cast<FuncDecl*>(stmt.get())) {
                functions[func->name] = func;
                function_return_types[func->name] = token_to_type(func->return_type);
            } else if (auto* load = dynamic_cast<LoadStmt*>(stmt.get())) {
                parse_and_register_module(load, src_file);
            } else if (auto* strct = dynamic_cast<StructDecl*>(stmt.get())) {
                structs[strct->name] = strct;
            }
        }

        // Compile globals
        for (const auto& stmt : program) {
            if (dynamic_cast<FuncDecl*>(stmt.get())) continue;
            compile_stmt(stmt.get(), bytecode);
        }

        // Jump over functions
        bytecode.emit(OpCode::OP_JUMP, 0, 0);
        size_t jump_placeholder = bytecode.code.size() - 2;

        // Compile non-main functions
        unordered_set<string> emitted_funcs;
        for (const auto& stmt : program) {
            if (auto* func = dynamic_cast<FuncDecl*>(stmt.get())) {
                if (func->name == "main") continue;
                emit_function_body(func, bytecode);
                emitted_funcs.insert(func->name);
            }
        }
        for (auto& [path, mod_program] : parsed_modules) {
            for (const auto& s : mod_program) {
                if (auto* func = dynamic_cast<FuncDecl*>(s.get())) {
                    if (emitted_funcs.count(func->name)) continue;
                    emit_function_body(func, bytecode);
                    emitted_funcs.insert(func->name);
                }
            }
        }

        // Patch jump to main
        if (functions.find("main") == functions.end()) {
            report_error(current_loc, "No 'main' function found.");
        }
        uint16_t main_addr = (uint16_t)bytecode.code.size();
        bytecode.function_addresses["main"] = main_addr;
        bytecode.code[jump_placeholder] = (main_addr >> 8) & 0xFF;
        bytecode.code[jump_placeholder + 1] = main_addr & 0xFF;

        // Compile main body. Unlike regular functions, main does not need an
        // implicit OP_RETURN: the OP_HALT below guarantees execution stops
        for (const auto& stmt : functions["main"]->body) {
            compile_stmt(stmt.get(), bytecode);
        }

        bytecode.emit(OpCode::OP_HALT);
        return;
    }
};