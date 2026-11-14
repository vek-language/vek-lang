#pragma once
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>
#include <memory>
#include "token.hpp"
#include "AST.hpp"

using std::runtime_error;
using std::vector;
using std::string;
using std::unique_ptr;
using std::make_unique;
using std::move;

class Parser {
public:
    explicit Parser(const vector<Token>& tkns) : tokens(tkns) {}

    vector<unique_ptr<Stmt>> parse_program() {
        vector<unique_ptr<Stmt>> statements;
        while (!is_at_end()) statements.push_back(parse_statement());
        return statements;
    }

private:
    const vector<Token>& tokens;
    size_t current = 0;

    // Helper functions
    bool is_at_end() const { return current >= tokens.size(); }
    bool check(TokenType type) const { return !is_at_end() && tokens[current].type == type; }
    Token previous() const { return tokens[current - 1]; }
    bool match(TokenType type) { if (!check(type)) return false; ++current; return true; }
    Token consume(TokenType expected, const string& message) {
        if (!check(expected)) throw runtime_error(message);
        return tokens[current++];
    }

    // Works just like declaration, but doesn't consumes semicolon
    unique_ptr<Stmt> parse_for_initializer() {
        if (check(TokenType::LET) || check(TokenType::CONST)) {
            bool is_const = match(TokenType::CONST);
            if (!is_const) consume(TokenType::LET, "Expected 'let'");
            
            TokenType type = parse_type();
            Token name = consume(TokenType::IDENTIFIER, "Expected name");
            consume(TokenType::EQUALS, "Expected '='");
            auto init_expr = parse_logic();
            
            return make_unique<VarDecl>(type, name.text, false, is_const, nullptr, move(init_expr));
        }
        
        return make_unique<ExprStmt>(parse_logic());
    }
    
    // x += N: x = x + N (WITHOUT SEMICOLON)
    unique_ptr<Expr> parse_quick_assignment(unique_ptr<Expr> expr) {
        if (match(TokenType::PLUS_EQUALS) || match(TokenType::MINUS_EQUALS) || 
            match(TokenType::STAR_EQUALS) || match(TokenType::SLASH_EQUALS)) {
            
            TokenType op = previous().type;
            TokenType math_op;

            if (op == TokenType::PLUS_EQUALS) math_op = TokenType::PLUS;
            else if (op == TokenType::MINUS_EQUALS) math_op = TokenType::MINUS;
            else if (op == TokenType::STAR_EQUALS) math_op = TokenType::STAR;
            else math_op = TokenType::SLASH;

            auto value = parse_logic();

            auto* var_target = dynamic_cast<VariableExpr*>(expr.get());
            if (!var_target) {
                throw runtime_error("Invalid target for compound assignment");
            }

            return make_unique<AssignExpr>(var_target->name, math_op, move(value));
        }
        return expr;
    }

    unique_ptr<Expr> parse_logic() {
        auto expr = parse_expression();
        while (match(TokenType::AND) || match(TokenType::OR)) {
            TokenType op = previous().type;
            auto right = parse_expression();
            expr = make_unique<BinaryExpr>(move(expr), op, move(right));
        }
        return expr;
    }

    // Parsing
    unique_ptr<Stmt> parse_statement() {
        if (match(TokenType::LOAD)) {
            string path;
            bool is_stdlib = false;

            if (match(TokenType::STR_LITERAL)) {
                // User files
                path = previous().text;
                is_stdlib = false;
            } else if (match(TokenType::IDENTIFIER)) {
                // Standard lib files
                path = previous().text;
                is_stdlib = true;
            } else {
                throw runtime_error("Expected file path or module name after 'load'");
            }
            
            consume(TokenType::SEMICOLON, "Expected ';' after load");
            return make_unique<LoadStmt>(path, is_stdlib);
        }

        if (check(TokenType::LET) || check(TokenType::CONST)) {
            return parse_declaration();
        }
        
        if (check(TokenType::STRUCT)) return parse_struct();
        if (check(TokenType::CLASS)) return parse_class();
        if (check(TokenType::FUNC)) return parse_function();
        if (check(TokenType::IF)) return parse_if();
        if (check(TokenType::WHILE)) return parse_while();
        if (check(TokenType::FOR)) return parse_for();

        if (match(TokenType::PRINT)) {
            consume(TokenType::LPAREN, "Expected '(' after 'print'");
            auto expr = parse_logic();
            consume(TokenType::RPAREN, "Expected ')' after print expression");
            consume(TokenType::SEMICOLON, "Expected ';'");
            return make_unique<PrintStmt>(move(expr));
        }

        if (check(TokenType::RET)) {
            consume(TokenType::RET, "Expected 'ret'");
            unique_ptr<Expr> value = nullptr;
            if (!check(TokenType::SEMICOLON)) value = parse_logic();
            consume(TokenType::SEMICOLON, "Expected ';'");
            return make_unique<ReturnStmt>(move(value));
        }

        if (check(TokenType::LBRACE)) return parse_block();

        auto expr = parse_logic();
        auto quick_assign = parse_quick_assignment(move(expr));

        if (dynamic_cast<AssignExpr*>(quick_assign.get())) {
            consume(TokenType::SEMICOLON, "Expected ';'");
            return make_unique<ExprStmt>(move(quick_assign));
        }
        
        if (match(TokenType::EQUALS)) {
            auto value = parse_logic();
            consume(TokenType::SEMICOLON, "Expected ';'");
            return make_unique<AssignStmt>(move(quick_assign), move(value));
        }
        if (match(TokenType::SEMICOLON)) {
            return make_unique<ExprStmt>(move(quick_assign));
        }
        throw runtime_error("Parsing failed on token: " + tokens[current].text + " (Previous: " + previous().text + ")");
    }

    unique_ptr<Stmt> parse_block() {
        consume(TokenType::LBRACE, "Expected '{'");
        vector<unique_ptr<Stmt>> body;
        while (!check(TokenType::RBRACE) && !is_at_end()) 
            body.push_back(parse_statement());

        consume(TokenType::RBRACE, "Expected '}'");
        return make_unique<BlockStmt>(move(body));
    }

    unique_ptr<Stmt> parse_class_member() {
        if (check(TokenType::FUNC)) {
            return parse_function();
        }
        
        return parse_declaration();
    }
    
    unique_ptr<Stmt> parse_class() {
        consume(TokenType::CLASS, "Expected 'class'");

        Token name = consume(TokenType::IDENTIFIER, "Excepted class name");
        consume(TokenType::LBRACE, "Expected '{'");

        auto class_decl = make_unique<ClassDecl>(name.text);
        while (!check(TokenType::RBRACE) && !is_at_end()) {
            // Access modifiers
            if(match(TokenType::PUBLIC)) {
                consume(TokenType::LBRACE, "Expected '{' after 'public'");
                while (!check(TokenType::RBRACE) && !is_at_end()) {
                    class_decl->public_members.push_back(parse_class_member());
                }
                consume(TokenType::RBRACE, "Expected '}' to close 'public' block");

            } else if(match(TokenType::PRIVATE)) {
                consume(TokenType::LBRACE, "Expected '{' after 'private'");
                while (!check(TokenType::RBRACE) && !is_at_end()) {
                    class_decl->private_members.push_back(parse_class_member());
                }
                consume(TokenType::RBRACE, "Expected '}' to close 'private' block");
            
            } else {
                throw runtime_error("Expected 'private' or 'public' block inside class");
            }
        }

        consume(TokenType::RBRACE, "Expected '}' after class declaration");
        return class_decl;
    }

    unique_ptr<Stmt> parse_struct() {
        consume(TokenType::STRUCT, "Expected 'struct'");
        Token name = consume(TokenType::IDENTIFIER, "Expected struct name");
        consume(TokenType::LBRACE, "Expected '{'");

        vector<Param> fields;
        while (!check(TokenType::RBRACE) && !is_at_end()) {
            consume(TokenType::LET, "Expected 'let' for struct field");
            TokenType type = parse_type();
            Token field_name = consume(TokenType::IDENTIFIER, "Expected field name");
            consume(TokenType::SEMICOLON, "Expected ';'");
            fields.emplace_back(type, field_name.text);
        }

        consume(TokenType::RBRACE, "Expected '}'");
        return make_unique<StructDecl>(name.text, move(fields));
    }
    
    unique_ptr<Stmt> parse_if() {
        consume(TokenType::IF, "Expected 'if'");
        consume(TokenType::LPAREN, "Expected '('");
        auto condition = parse_logic();
        consume(TokenType::RPAREN, "Expected ')'");
        
        auto then_branch = parse_block();

        unique_ptr<Stmt> else_branch = nullptr;
        if (match(TokenType::ELSE)) {
            if (check(TokenType::IF)) {
                else_branch = parse_if();
            } else {
                else_branch = parse_block();
            }
        }

        return make_unique<IfStmt>(move(condition), move(then_branch), move(else_branch));
    }

    unique_ptr<Stmt> parse_while() {
        consume(TokenType::WHILE, "Expected 'while'");
        consume(TokenType::LPAREN, "Expected '('");
        auto condition = parse_logic();
        consume(TokenType::RPAREN, "Expected ')'");
        
        // Body
        auto while_branch = parse_block();
        return make_unique<WhileStmt>(move(condition), move(while_branch));
    }

    unique_ptr<Stmt> parse_for() {
        consume(TokenType::FOR, "Expected 'for'");
        consume(TokenType::LPAREN, "Expected '('");
        
        // Index
        unique_ptr<Stmt> initializer = nullptr;
        if (!check(TokenType::SEMICOLON)) {
            if (check(TokenType::LET) || check(TokenType::CONST)) {
                initializer = parse_for_initializer(); 
            } else {
                auto expr = parse_logic();
                initializer = make_unique<ExprStmt>(move(expr));
            }
        } 
        consume(TokenType::SEMICOLON, "Expected ';' after loop initializer");

        // Condition
        unique_ptr<Expr> condition = nullptr;
        if (!check(TokenType::SEMICOLON)) {
            condition = parse_logic();
        }
        consume(TokenType::SEMICOLON, "Expected ';' after loop condition");

        // Increment
        unique_ptr<Expr> increment = nullptr;
        if (!check(TokenType::RPAREN)) {
            auto expr = parse_logic();
            increment = parse_quick_assignment(move(expr));
        }
        consume(TokenType::RPAREN, "Expected ')' after 'for'");

        // Body
        auto body = parse_block();
        return make_unique<ForStmt>(move(initializer), move(condition), move(increment), move(body));
    }

    unique_ptr<Stmt> parse_function() {
        consume(TokenType::FUNC, "Expected 'func'");
        Token name = consume(TokenType::IDENTIFIER, "Expected function name");
        consume(TokenType::LPAREN, "Expected '('");
        
        vector<Param> params;
        if (!check(TokenType::RPAREN)) {
            do {
                bool is_ref = match(TokenType::REF);
                TokenType p_type = parse_type();
                Token p_name = consume(TokenType::IDENTIFIER, "Expected parameter name");
                params.emplace_back(p_type, p_name.text, is_ref);
            } while (match(TokenType::COMMA));
        }
        consume(TokenType::RPAREN, "Expected ')'");
        TokenType return_type = parse_type();
        
        consume(TokenType::LBRACE, "Expected '{'");
        vector<unique_ptr<Stmt>> body;
        while (!check(TokenType::RBRACE) && !is_at_end()) {
            body.push_back(parse_statement());
        }
        consume(TokenType::RBRACE, "Expected '}'");

        return make_unique<FuncDecl>(return_type, name.text, move(params), move(body));
    }

    TokenType parse_type() {
        if (match(TokenType::INT32)) return TokenType::INT32;
        if (match(TokenType::INT64)) return TokenType::INT64;
        if (match(TokenType::FLOAT32)) return TokenType::FLOAT32;
        if (match(TokenType::FLOAT64)) return TokenType::FLOAT64;
        if (match(TokenType::STR)) return TokenType::STR;
        if (match(TokenType::CHAR)) return TokenType::CHAR;
        if (match(TokenType::BOOL)) return TokenType::BOOL;
        if (match(TokenType::VOID)) return TokenType::VOID;

        if (match(TokenType::IDENTIFIER)) {
            return TokenType::IDENTIFIER; 
        }

        throw runtime_error("Expected a type");
    }

    TokenType parse_decl_type(string& out_class_name) {
        out_class_name.clear();
        if (check(TokenType::INT32) 
            || check(TokenType::INT64) 
            || check(TokenType::FLOAT32) 
            || check(TokenType::FLOAT64)
            || check(TokenType::STR)
            || check(TokenType::CHAR)
            || check(TokenType::BOOL)
            || check(TokenType::VOID))
        {
            return parse_type();
        }
        if (check(TokenType::IDENTIFIER)) {
            out_class_name = tokens[current].text;
            ++current;
            return TokenType::IDENTIFIER; 
        }
        throw runtime_error("Expected a type or class name");
    }

    unique_ptr<Stmt> parse_declaration() {
        bool is_const = false;
        
        if (match(TokenType::CONST)) {
            is_const = true;
        } else {
            consume(TokenType::LET, "Expected 'let' or 'const'");
        }

        TokenType type = parse_type();
        Token name = consume(TokenType::IDENTIFIER, "Expected a variable name");
        
        bool is_array = false;
        unique_ptr<Expr> array_size = nullptr;
        
        if (match(TokenType::LBRACKET)) {
            is_array = true;
            if (!check(TokenType::RBRACKET)) {
                array_size = parse_logic();
            }
            consume(TokenType::RBRACKET, "Expected ']'");
        }

        unique_ptr<Expr> initializer = nullptr;
        if (match(TokenType::EQUALS)) {
            initializer = parse_logic();
        } else if (is_const) {
            throw runtime_error("Constants must be initialized on declaration");
        } else {
             if (!is_array) {
                if (type == TokenType::INT32 || type == TokenType::INT64) {
                    initializer = make_unique<Int64Literal>(0);
                } else if (type == TokenType::FLOAT32 || type == TokenType::FLOAT64) {
                    initializer = make_unique<Float64Literal>(0.0);
                } else if (type == TokenType::BOOL) {
                    initializer = make_unique<BoolLiteral>(false);
                } else if (type == TokenType::STR) {
                    initializer = make_unique<StringLiteral>("");
                } else if (type == TokenType::CHAR) {
                    initializer = make_unique<CharLiteral>('\0');
                }
            }
        }
        
        consume(TokenType::SEMICOLON, "Expected ';'");
        return make_unique<VarDecl>(type, name.text, is_array, is_const, move(array_size), move(initializer));
    }

    unique_ptr<Expr> parse_expression() {
        auto expr = parse_term();
        while (match(TokenType::PLUS) 
            || match(TokenType::MINUS) 
            || match(TokenType::GREATER_THAN)
            || match(TokenType::LESS_THAN) 
            || match(TokenType::GREATER_EQUAL)
            || match(TokenType::LESS_EQUAL)
            || match(TokenType::EQUALS_EQUALS) 
            || match(TokenType::NOT_EQUALS))
        {
            TokenType op = previous().type;
            auto right = parse_term();
            expr = make_unique<BinaryExpr>(move(expr), op, move(right));
        }
        return expr;
    }

    unique_ptr<Expr> parse_term() {
        auto expr = parse_factor();
        while (match(TokenType::STAR) || match(TokenType::SLASH)) {
            TokenType op = previous().type;
            auto right = parse_factor();
            expr = make_unique<BinaryExpr>(move(expr), op, move(right));
        }
        return expr;
    }

    unique_ptr<Expr> parse_factor() {
        auto expr = parse_primary();

        while (true) {
            if (match(TokenType::LBRACKET)) {
                auto index = parse_logic();
                consume(TokenType::RBRACKET, "Expected ']'");
                expr = make_unique<ArrayAccessExpr>(move(expr), move(index));
            } else if (match(TokenType::DOT)) {
                Token name = consume(TokenType::IDENTIFIER, "Expected field or method name");
                
                if (match(TokenType::LPAREN)) {
                    // It's a method call
                    vector<unique_ptr<Expr>> args;
                    if (!check(TokenType::RPAREN)) {
                        do { args.push_back(parse_logic()); } while (match(TokenType::COMMA));
                    }
                    consume(TokenType::RPAREN, "Expected ')'");
                    expr = make_unique<MethodCallExpr>(move(expr), name.text, move(args));
                } else {
                    // It's a field access
                    expr = make_unique<FieldAccessExpr>(move(expr), name.text);
                }
            } else {
                break;
            }
        }
        return expr;
    }

    unique_ptr<Expr> parse_primary() {
        if (match(TokenType::NOT)) {
            TokenType op = previous().type;
            auto right = parse_primary();
            
            return make_unique<UnaryExpr>(op, move(right));
        }

        if (match(TokenType::MINUS)) {
            auto expr = parse_primary();
            
            // turns into `0 - number`
            return make_unique<BinaryExpr>(
                make_unique<Int64Literal>(0),
                TokenType::MINUS,
                move(expr)
            );
        }
        
        if (match(TokenType::INT64_LITERAL)) {
            return make_unique<Int64Literal>(std::stoll(previous().text));
        }
        
        if (match(TokenType::FLOAT64_LITERAL)) {
            string text = previous().text;
            if (!text.empty() && (text.back() == 'd' || text.back() == 'D')) {
                text.pop_back();
            }
            return make_unique<Float64Literal>(std::stod(text));
        }

        if (match(TokenType::FLOAT32_LITERAL)) {
            string text = previous().text;
            if (!text.empty() && (text.back() == 'f' || text.back() == 'F')) {
                text.pop_back();
            }
            return make_unique<Float32Literal>(std::stof(text));
        }

        if (match(TokenType::BOOL_LITERAL)) return make_unique<BoolLiteral>(previous().text == "true");
        if (match(TokenType::STR_LITERAL)) return make_unique<StringLiteral>(previous().text);
        if (match(TokenType::CHAR_LITERAL)) return make_unique<CharLiteral>(previous().text[0]);

        if (match(TokenType::LBRACE)) {
            // Check for struct literal
            if (check(TokenType::IDENTIFIER) && current + 1 < tokens.size() && tokens[current + 1].type == TokenType::COLON) {
                vector<pair<string, unique_ptr<Expr>>> inits;
                do {
                    Token field_name = consume(TokenType::IDENTIFIER, "Expected field name");
                    consume(TokenType::COLON, "Expected ':'");
                    auto val = parse_logic();
                    inits.emplace_back(field_name.text, move(val));
                } while (match(TokenType::COMMA));
                consume(TokenType::RBRACE, "Expected '}'");
                return make_unique<StructLiteral>("", move(inits)); // Name inferred by compiler
            }

            // Array literal
            vector<unique_ptr<Expr>> elements;
            if (!check(TokenType::RBRACE)) {
                do {
                    elements.push_back(parse_logic());
                } while (match(TokenType::COMMA));
            }
            consume(TokenType::RBRACE, "Expected '}'");
            return make_unique<ArrayLiteral>(move(elements));
        }

        if (match(TokenType::IDENTIFIER)) {
            string name = previous().text;
            if (match(TokenType::LPAREN)) {
                vector<unique_ptr<Expr>> args;
                if (!check(TokenType::RPAREN)) {
                    do { args.push_back(parse_logic()); } while (match(TokenType::COMMA));
                }
                consume(TokenType::RPAREN, "Expected ')'");
                return make_unique<CallExpr>(name, move(args));
            }
            return make_unique<VariableExpr>(name);
        }

        if (match(TokenType::LPAREN)) {
            // cast check
            if (check(TokenType::INT32) || check(TokenType::INT64) || 
                check(TokenType::FLOAT32) || check(TokenType::FLOAT64) || 
                check(TokenType::STR) || check(TokenType::CHAR) || 
                check(TokenType::BOOL)) {
                
                TokenType target_type = parse_type();
                consume(TokenType::RPAREN, "Expected ')' after cast type");
                auto expr = parse_primary();
                return make_unique<CastExpr>(target_type, move(expr));
            }
            
            // Fallback to grouping
            auto expr = parse_logic();
            consume(TokenType::RPAREN, "Expected ')'");
            return expr;
        }
        throw runtime_error("Expected a value, variable, or '('");
    }
};