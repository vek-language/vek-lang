#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include "AST.hpp"
#include "bytecode.hpp"
#include "token.hpp"

using std::runtime_error;
using std::vector;
using std::unique_ptr;
using std::string;
using std::unordered_map;
using std::unordered_set;

enum class Type { INT32, INT64, FLOAT32, FLOAT64, BOOL, CHAR, STR, VOID, ARRAY, UNKNOWN };

class Compiler {
private:
    unordered_map<string, FuncDecl*> functions;
    unordered_map<string, Type> function_return_types;

    vector<unordered_map<string, Type>> variable_scopes;
    vector<unordered_map<string, bool>> constant_scopes;

    unordered_map<string, vector<unique_ptr<Stmt>>> parsed_modules;
    unordered_set<string> registered_modules;
    unordered_set<string> emitted_modules;

    // Scope helpers
    void push_scope() { 
        variable_scopes.push_back({}); 
        constant_scopes.push_back({});
    }
    void pop_scope() { 
        variable_scopes.pop_back(); 
        constant_scopes.pop_back();
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

    // Type helpers
    Type token_to_type(TokenType t) {
        switch(t) {
            case TokenType::INT32: return Type::INT32;
            case TokenType::INT64: return Type::INT64;
            case TokenType::FLOAT32: return Type::FLOAT32;
            case TokenType::FLOAT64: return Type::FLOAT64;
            case TokenType::BOOL: return Type::BOOL;
            case TokenType::CHAR: return Type::CHAR;
            case TokenType::STR: return Type::STR;
            case TokenType::VOID: return Type::VOID;
            default: return Type::UNKNOWN;
        }
    }
    
    string type_to_string(Type t) { 
        switch(t) {
            case Type::INT32: return "int32"; 
            case Type::INT64: return "int64";
            case Type::FLOAT32: return "float32"; 
            case Type::FLOAT64: return "float64";
            case Type::BOOL: return "bool"; 
            case Type::CHAR: return "char";
            case Type::STR: return "str"; 
            case Type::VOID: return "void";
            case Type::ARRAY: return "array";
            default: return "unknown";
        }
    }

    bool is_numeric(Type t) { return t == Type::INT32 || t == Type::INT64 || t == Type::FLOAT32 || t == Type::FLOAT64; }
    bool is_assignable(Type target, Type source) { return target == source || (is_numeric(target) && is_numeric(source)); }

    void patch_jump(Bytecode& bytecode, size_t jump_index, size_t target) {
        bytecode.code[jump_index] = (target >> 8) & 0xFF;
        bytecode.code[jump_index + 1] = target & 0xFF;
    }

    // Module loading
    string resolve_load_path(LoadStmt* stmt) {
        if (stmt->is_stdlib) return "libs/" + stmt->filepath + ".vek";
        return stmt->filepath + ".vek";
    }

    void parse_and_register_module(LoadStmt* stmt) {
        string full_path = resolve_load_path(stmt);
        if (registered_modules.count(full_path)) return;
        registered_modules.insert(full_path);

        std::ifstream file(full_path);
        if (!file.is_open()) {
            throw runtime_error("Could not find module: " + stmt->filepath);
        }
        std::stringstream buffer;
        buffer << file.rdbuf();

        Lexer lexer(buffer.str());
        auto tokens = lexer.lex();
        Parser parser(tokens);
        auto program = parser.parse_program();

        for (const auto& s : program) {
            if (auto* load = dynamic_cast<LoadStmt*>(s.get())) {
                parse_and_register_module(load);
            }
        }

        for (const auto& s : program) {
            if (auto* func = dynamic_cast<FuncDecl*>(s.get())) {
                if (func->name == "main") {
                    throw runtime_error("Modules cannot define 'main'.");
                }
                functions[func->name] = func;
                function_return_types[func->name] = token_to_type(func->return_type);
            }
        }

        parsed_modules[full_path] = std::move(program);
    }

    void emit_module_globals(LoadStmt* stmt, Bytecode& bytecode) {
        string full_path = resolve_load_path(stmt);
        parse_and_register_module(stmt);

        if (emitted_modules.count(full_path)) return;
        emitted_modules.insert(full_path);

        const auto& program = parsed_modules[full_path];

        for (const auto& s : program) {
            if (auto* load = dynamic_cast<LoadStmt*>(s.get())) {
                emit_module_globals(load, bytecode);
            }
        }

        for (const auto& s : program) {
            if (dynamic_cast<FuncDecl*>(s.get()) || dynamic_cast<LoadStmt*>(s.get())) continue;
            compile_stmt(s.get(), bytecode);
        }
    }

    // Conservative: returns true only when every path provably ends in 'ret'.
    // Unknown shapes return false; callers then emit an implicit default return.
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
            int name_idx = bytecode.add_const(param.name);
            bytecode.emit(param.is_ref ? OpCode::OP_DEFINE_REF_VAR : OpCode::OP_DEFINE_VAR, name_idx);
            set_var_type(param.name, token_to_type(param.type));
        }
        
        for (const auto& body_stmt : func->body) {
            compile_stmt(body_stmt.get(), bytecode);
        }
        pop_scope();

        // Guarantee a return so control never falls through into the next function.
        // If the body does not provably end in 'ret', emit an implicit default return.
        if (func->body.empty() || !ends_with_return(func->body.back().get())) {
            int idx = bytecode.add_const((int64_t)0);
            bytecode.emit(OpCode::OP_CONSTANT, idx);
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
        compile_expr(stmt->condition.get(), bytecode, Type::BOOL);

        bytecode.emit(OpCode::OP_JUMP_IF_FALSE, 0, 0);
        size_t exit_jump = bytecode.code.size() - 2;

        compile_stmt(stmt->body.get(), bytecode);

        bytecode.emit(OpCode::OP_JUMP, (loop_start >> 8) & 0xFF, loop_start & 0xFF);
        patch_jump(bytecode, exit_jump, bytecode.code.size());
    }

    void compile_for(ForStmt* stmt, Bytecode& bytecode) {
        bytecode.emit(OpCode::OP_ENTER_SCOPE);
        push_scope();

        if (stmt->initializer) {
            compile_stmt(stmt->initializer.get(), bytecode);
        }

        size_t loop_start = bytecode.code.size();

        size_t exit_jump = 0;
        if (stmt->condition) {
            compile_expr(stmt->condition.get(), bytecode, Type::BOOL);
            bytecode.emit(OpCode::OP_JUMP_IF_FALSE, 0, 0);
            exit_jump = bytecode.code.size() - 2;
        }

        compile_stmt(stmt->body.get(), bytecode);

        if (stmt->increment) {
            compile_expr(stmt->increment.get(), bytecode, Type::UNKNOWN);
            if (!dynamic_cast<AssignExpr*>(stmt->increment.get())) {
                bytecode.emit(OpCode::OP_POP);
            }
        }

        bytecode.emit(OpCode::OP_JUMP, (loop_start >> 8) & 0xFF, loop_start & 0xFF);

        if (stmt->condition) {
            patch_jump(bytecode, exit_jump, bytecode.code.size());
        }

        bytecode.emit(OpCode::OP_EXIT_SCOPE);
        pop_scope();
    }

    // Statements
    void compile_var_decl(VarDecl* var_decl, Bytecode& bytecode) {
        Type declared_type = token_to_type(var_decl->type);
        set_var_type(var_decl->name, var_decl->is_array ? Type::ARRAY : declared_type);
        set_const(var_decl->name, var_decl->is_const);

        if (var_decl->is_array) {
            bool handled = false;
            
            if (auto* arr_lit = dynamic_cast<ArrayLiteral*>(var_decl->initializer.get())) {
                int64_t size = arr_lit->elements.size();
                int idx = bytecode.add_const(size);
                bytecode.emit(OpCode::OP_CONSTANT, idx);
                
                bytecode.emit(OpCode::OP_NEW_ARRAY);
                int name_index = bytecode.add_const(var_decl->name);
                bytecode.emit(OpCode::OP_DEFINE_VAR, name_index);

                for (size_t i = 0; i < arr_lit->elements.size(); ++i) {
                    int var_idx = bytecode.add_const(var_decl->name);
                    bytecode.emit(OpCode::OP_LOAD, var_idx);

                    int idx_val = bytecode.add_const((int64_t)i);
                    bytecode.emit(OpCode::OP_CONSTANT, idx_val);

                    compile_expr(arr_lit->elements[i].get(), bytecode, declared_type);
                    bytecode.emit(OpCode::OP_SET_INDEX);
                }
                handled = true;
            } 
            else if (var_decl->array_size) {
                compile_expr(var_decl->array_size.get(), bytecode, Type::INT64);
                bytecode.emit(OpCode::OP_NEW_ARRAY);
                int name_index = bytecode.add_const(var_decl->name);
                bytecode.emit(OpCode::OP_DEFINE_VAR, name_index);
                handled = true;
            }
            else if (var_decl->initializer) {
                compile_expr(var_decl->initializer.get(), bytecode, Type::ARRAY);
                int name_index = bytecode.add_const(var_decl->name);
                bytecode.emit(OpCode::OP_DEFINE_VAR, name_index);
                handled = true;
            }

            if (!handled) {
                throw runtime_error("Array size error. Provide [N] or an initializer.");
            }
        } else {
            compile_expr(var_decl->initializer.get(), bytecode, declared_type);
            int name_index = bytecode.add_const(var_decl->name);
            bytecode.emit(OpCode::OP_DEFINE_VAR, name_index);
        }
    }

    void compile_assign(AssignStmt* assign_stmt, Bytecode& bytecode) {
        if (auto* var = dynamic_cast<VariableExpr*>(assign_stmt->target.get())) {
            if (is_constant(var->name)) {
                throw runtime_error("Cannot reassign constant variable '" + var->name + "'");
            }

            Type target_type = get_var_type(var->name);
            compile_expr(assign_stmt->value.get(), bytecode, target_type);
            int name_index = bytecode.add_const(var->name);
            bytecode.emit(OpCode::OP_SAVE, name_index);
        } else if (auto* arr_access = dynamic_cast<ArrayAccessExpr*>(assign_stmt->target.get())) {
            compile_expr(arr_access->array.get(), bytecode);
            compile_expr(arr_access->index.get(), bytecode);
            compile_expr(assign_stmt->value.get(), bytecode);
            bytecode.emit(OpCode::OP_SET_INDEX);
        } else {
            throw runtime_error("Invalid assignment target");
        }
    }

    void compile_print(PrintStmt* print_stmt, Bytecode& bytecode) {
        compile_expr(print_stmt->expression.get(), bytecode, Type::UNKNOWN);
        bytecode.emit(OpCode::OP_PRINT);
    }

    void compile_return(ReturnStmt* ret_stmt, Bytecode& bytecode) {
        if (ret_stmt->value) {
            compile_expr(ret_stmt->value.get(), bytecode, Type::UNKNOWN);
        } else {
            int idx = bytecode.add_const((int64_t)0);
            bytecode.emit(OpCode::OP_CONSTANT, idx);
        }
        bytecode.emit(OpCode::OP_RETURN);
    }

    // Dispatcher
    void compile_stmt(Stmt* stmt, Bytecode& bytecode) {
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

        if (auto* load_stmt = dynamic_cast<LoadStmt*>(stmt)) { emit_module_globals(load_stmt, bytecode); return; }
        if (auto* if_stmt = dynamic_cast<IfStmt*>(stmt)) { compile_if(if_stmt, bytecode); return; }
        if (auto* while_stmt = dynamic_cast<WhileStmt*>(stmt)) { compile_while(while_stmt, bytecode); return; }
        if (auto* for_stmt = dynamic_cast<ForStmt*>(stmt)) { compile_for(for_stmt, bytecode); return; }

        if (auto* var_decl = dynamic_cast<VarDecl*>(stmt)) { compile_var_decl(var_decl, bytecode); return; }
        if (auto* assign_stmt = dynamic_cast<AssignStmt*>(stmt)) { compile_assign(assign_stmt, bytecode); return; }
        if (auto* print_stmt = dynamic_cast<PrintStmt*>(stmt)) { compile_print(print_stmt, bytecode); return; }
        if (auto* ret_stmt = dynamic_cast<ReturnStmt*>(stmt)) { compile_return(ret_stmt, bytecode); return; }

        if (auto* func = dynamic_cast<FuncDecl*>(stmt)) { emit_function_body(func, bytecode); return; }

        if (auto* expr_stmt = dynamic_cast<ExprStmt*>(stmt)) {
            compile_expr(expr_stmt->expression.get(), bytecode, Type::UNKNOWN);
            if (!dynamic_cast<AssignExpr*>(expr_stmt->expression.get())) {
                bytecode.emit(OpCode::OP_POP);
            }
            return;
        }
        
        throw runtime_error("Unknown statement type");
    }

    // Expressions
    Type compile_expr(Expr* expr, Bytecode& bytecode, Type expected_type = Type::UNKNOWN) {
        if (auto* i64 = dynamic_cast<Int64Literal*>(expr)) {
            if (expected_type == Type::INT32) {
                int idx = bytecode.add_const(static_cast<int32_t>(i64->value));
                bytecode.emit(OpCode::OP_CONSTANT, idx);
                return Type::INT32;
            }

            int idx = bytecode.add_const(static_cast<int64_t>(i64->value));
            bytecode.emit(OpCode::OP_CONSTANT, idx);
            return Type::INT64;
        }
        if (auto* f64 = dynamic_cast<Float64Literal*>(expr)) {
            int idx = bytecode.add_const(static_cast<double>(f64->value));
            bytecode.emit(OpCode::OP_CONSTANT, idx);
            return Type::FLOAT64;
        }
        if (auto* f32 = dynamic_cast<Float32Literal*>(expr)) {
            int idx = bytecode.add_const(static_cast<float>(f32->value));
            bytecode.emit(OpCode::OP_CONSTANT, idx);
            return Type::FLOAT32;
        }
        if (auto* b = dynamic_cast<BoolLiteral*>(expr)) {
            int idx = bytecode.add_const(b->value);
            bytecode.emit(OpCode::OP_CONSTANT, idx);
            return Type::BOOL;
        }
        if (auto* s = dynamic_cast<StringLiteral*>(expr)) {
            int idx = bytecode.add_const(s->value);
            bytecode.emit(OpCode::OP_CONSTANT, idx);
            return Type::STR;
        }
        if (auto* c = dynamic_cast<CharLiteral*>(expr)) {
            int idx = bytecode.add_const(c->value);
            bytecode.emit(OpCode::OP_CONSTANT, idx);
            return Type::CHAR;
        }
        
        if (auto* var = dynamic_cast<VariableExpr*>(expr)) {
            int idx = bytecode.add_const(var->name);
            bytecode.emit(OpCode::OP_LOAD, idx);
            return get_var_type(var->name);
        }

        if (auto* assign_expr = dynamic_cast<AssignExpr*>(expr)) {
            if (is_constant(assign_expr->name)) {
                throw runtime_error("Cannot reassign constant variable '" + assign_expr->name + "'");
            }
            
            int load_idx = bytecode.add_const(assign_expr->name);
            bytecode.emit(OpCode::OP_LOAD, load_idx);
            compile_expr(assign_expr->value.get(), bytecode, Type::UNKNOWN);

            switch (assign_expr->op) {
                case TokenType::PLUS:  bytecode.emit(OpCode::OP_ADD); break;
                case TokenType::MINUS: bytecode.emit(OpCode::OP_SUBTRACT); break;
                case TokenType::STAR:  bytecode.emit(OpCode::OP_MULTIPLY); break;
                case TokenType::SLASH: bytecode.emit(OpCode::OP_DIVIDE); break;
                default: throw runtime_error("Invalid assignment operator");
            }

            bytecode.emit(OpCode::OP_SAVE, load_idx);
            return Type::UNKNOWN;
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
                if (method_call->arguments.size() != 1) throw runtime_error("push expects 1 argument");
                bytecode.emit(OpCode::OP_ARRAY_PUSH);
            } 
            else if (method_call->method == "pop") {
                if (!method_call->arguments.empty()) throw runtime_error("pop expects 0 arguments");
                bytecode.emit(OpCode::OP_ARRAY_POP);
            } 
            else {
                throw runtime_error("Unknown array method: " + method_call->method);
            }
            return Type::UNKNOWN;
        }

        if (auto* call = dynamic_cast<CallExpr*>(expr)) {
            FuncDecl* fn = nullptr;
            if (functions.count(call->name)) fn = functions[call->name];

            if (fn && fn->params.size() != call->arguments.size()) {
                throw runtime_error("Function '" + call->name + "' expects " +
                    std::to_string(fn->params.size()) + " args, got " +
                    std::to_string(call->arguments.size()));
            }

            for (size_t i = 0; i < call->arguments.size(); i++) {
                size_t idx = call->arguments.size() - 1 - i;   // reverse iteration
                auto& arg = call->arguments[idx];
                bool is_ref = fn && fn->params[idx].is_ref;

                if (is_ref) {
                    auto* v = dynamic_cast<VariableExpr*>(arg.get());
                    if (!v) throw runtime_error("Ref argument must be a variable");
                    int name_idx = bytecode.add_const(v->name);
                    bytecode.emit(OpCode::OP_LOAD_REF, name_idx);
                } else {
                    compile_expr(arg.get(), bytecode, Type::UNKNOWN);
                }
            }

            int name_idx = bytecode.add_const(call->name);
            bytecode.emit(OpCode::OP_CALL, name_idx, call->arguments.size());
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
            
            // Short circuit
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
            
            Type left_type = compile_expr(bin->left.get(), bytecode, expected_type);
            Type right_type = compile_expr(bin->right.get(), bytecode, expected_type);
            
            Type result_type = Type::UNKNOWN;
            if (is_numeric(left_type) && is_numeric(right_type)) {
                if (left_type == Type::FLOAT64 || right_type == Type::FLOAT64) {
                    result_type = Type::FLOAT64;
                } 
                else if (left_type == Type::FLOAT32 || right_type == Type::FLOAT32) {
                    result_type = Type::FLOAT32;
                } 
                else if (left_type == Type::INT64 || right_type == Type::INT64) {
                    result_type = Type::INT64;
                } 
                else {
                    result_type = Type::INT32; 
                }
            } 
            else if (left_type == Type::STR && right_type == Type::STR && bin->op == TokenType::PLUS) {
                result_type = Type::STR;
            }

            if (bin->op == TokenType::PLUS) {
                bool is_valid_string = (left_type == Type::STR && right_type == Type::STR);
                bool is_valid_number = (is_numeric(left_type) && is_numeric(right_type));
                
                if (!is_valid_string && !is_valid_number) {
                    throw runtime_error("Cannot add " + type_to_string(left_type) + " and " + type_to_string(right_type));
                }
            } 
            else if (bin->op == TokenType::EQUALS_EQUALS || bin->op == TokenType::NOT_EQUALS) {
                bool is_valid_number = (is_numeric(left_type) && is_numeric(right_type));
                bool is_valid_string = (left_type == Type::STR && right_type == Type::STR);
                bool is_valid_char = (left_type == Type::CHAR && right_type == Type::CHAR);
                bool is_valid_bool = (left_type == Type::BOOL && right_type == Type::BOOL);

                if (!is_valid_number && !is_valid_string && !is_valid_char && !is_valid_bool) {
                    throw runtime_error("Cannot compare " + type_to_string(left_type) + " and " + type_to_string(right_type));
                }
                
                result_type = Type::BOOL;
            }
            else {
                if (!is_numeric(left_type) || !is_numeric(right_type)) {
                    throw runtime_error("Math operations require numbers, got " + type_to_string(left_type) + " and " + type_to_string(right_type));
                }
                
                if (left_type == Type::FLOAT64 || right_type == Type::FLOAT64) {
                    result_type = Type::FLOAT64;
                } else if (left_type == Type::FLOAT32 || right_type == Type::FLOAT32) {
                    result_type = Type::FLOAT32;
                } else if (left_type == Type::INT64 || right_type == Type::INT64) {
                    result_type = Type::INT64;
                } else {
                    result_type = Type::INT32;
                }
            }

            switch (bin->op) {
                case TokenType::PLUS: bytecode.emit(OpCode::OP_ADD); break;
                case TokenType::MINUS: bytecode.emit(OpCode::OP_SUBTRACT); break;
                case TokenType::STAR: bytecode.emit(OpCode::OP_MULTIPLY); break;
                case TokenType::SLASH: bytecode.emit(OpCode::OP_DIVIDE); break;
                case TokenType::GREATER_THAN: bytecode.emit(OpCode::OP_GREATER_THAN); break;
                case TokenType::LESS_THAN: bytecode.emit(OpCode::OP_LESS_THAN); break;
                case TokenType::GREATER_EQUAL: bytecode.emit(OpCode::OP_GREATER_EQUAL); break;
                case TokenType::LESS_EQUAL: bytecode.emit(OpCode::OP_LESS_EQUAL); break;
                case TokenType::EQUALS_EQUALS: bytecode.emit(OpCode::OP_EQUAL); break;
                case TokenType::NOT_EQUALS: 
                    bytecode.emit(OpCode::OP_EQUAL); 
                    bytecode.emit(OpCode::OP_NOT); 
                    break;

                default: throw runtime_error("Unknown binary operator");
            }
            return result_type;
        }
        throw runtime_error("Unknown expression type");
    }

public:
    Bytecode compile(const vector<unique_ptr<Stmt>>& program) {
        Bytecode bytecode;
        push_scope();

        // Register functions
        for (const auto& stmt : program) {
            if (auto* func = dynamic_cast<FuncDecl*>(stmt.get())) {
                functions[func->name] = func;
                function_return_types[func->name] = token_to_type(func->return_type);
            }
            if (auto* load = dynamic_cast<LoadStmt*>(stmt.get())) {
                parse_and_register_module(load);
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
            throw runtime_error("No 'main' function found.");
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
        return bytecode;
    }
};