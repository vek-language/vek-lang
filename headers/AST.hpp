#pragma once
#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include "token.hpp"

using std::string;
using std::vector;
using std::unique_ptr;
using std::move;

struct Expr { virtual ~Expr() = default; };

struct Int32Literal : Expr { int32_t value; Int32Literal(int32_t value) : value(value) {} };
struct Int64Literal : Expr { int64_t value; Int64Literal(int64_t value) : value(value) {} };
struct Float32Literal : Expr { float value; Float32Literal(float value) : value(value) {} };
struct Float64Literal : Expr { double value; Float64Literal(double value) : value(value) {} };
struct BoolLiteral : Expr { bool value; BoolLiteral(bool value) : value(value) {} };
struct StringLiteral : Expr { string value; StringLiteral(string value) : value(move(value)) {} };
struct CharLiteral : Expr { char value; CharLiteral(char value) : value(value) {} };

struct VariableExpr : Expr {
    string name;
    VariableExpr(string n) 
        : name(move(n)) {}
};

struct FieldAccessExpr : Expr {
    unique_ptr<Expr> object;
    string field;
    FieldAccessExpr(unique_ptr<Expr> obj, string f)
        : object(move(obj)), field(move(f)) {}
};

struct AssignExpr : Expr {
    string name;
    TokenType op;
    unique_ptr<Expr> value;

    AssignExpr(string n, TokenType operation, unique_ptr<Expr> value)
        : name(move(n)), op(operation), value(move(value)) {}
};

struct CastExpr : Expr {
    TokenType target_type;
    unique_ptr<Expr> expr;
    CastExpr(TokenType token_type, unique_ptr<Expr> e) 
        : target_type(token_type), expr(move(e)) {}
};

struct UnaryExpr : Expr {
    TokenType op;
    unique_ptr<Expr> right;
    UnaryExpr(TokenType operation, unique_ptr<Expr> r) 
        : op(operation), right(move(r)) {}
};

struct ArrayAccessExpr : Expr {
    unique_ptr<Expr> array;
    unique_ptr<Expr> index;
    ArrayAccessExpr(unique_ptr<Expr> a, unique_ptr<Expr> i) 
        : array(move(a)), index(move(i)) {}
};

struct ArrayLiteral : Expr {
    vector<unique_ptr<Expr>> elements;
    ArrayLiteral(vector<unique_ptr<Expr>> e) 
        : elements(move(e)) {}
};

struct MethodCallExpr : Expr {
    unique_ptr<Expr> object;
    string method;
    vector<unique_ptr<Expr>> arguments;
    
    MethodCallExpr(unique_ptr<Expr> obj, string m, vector<unique_ptr<Expr>> args) 
        : object(move(obj)), method(move(m)), arguments(move(args)) {}
};

struct CallExpr : Expr {
    string name;
    vector<unique_ptr<Expr>> arguments;
    CallExpr(string name, vector<unique_ptr<Expr>> args) 
        : name(move(name)), arguments(move(args)) {}
};

struct BinaryExpr : Expr {
    unique_ptr<Expr> left;
    TokenType op;
    unique_ptr<Expr> right;
    BinaryExpr(unique_ptr<Expr> l, TokenType operation, unique_ptr<Expr> r) 
        : left(move(l)), op(operation), right(move(r)) {}
};

struct Stmt { virtual ~Stmt() = default; };

struct LoadStmt : Stmt {
    string filepath;
    bool is_stdlib;
    LoadStmt(string path, bool stdlib) 
        : filepath(move(path)), is_stdlib(stdlib) {}
};

struct ExprStmt : Stmt {
    unique_ptr<Expr> expression;
    ExprStmt(unique_ptr<Expr> expr) 
        : expression(move(expr)) {}
};

struct VarDecl : Stmt {
    TokenType type;
    string name;
    bool is_array;
    bool is_const;
    unique_ptr<Expr> array_size;
    unique_ptr<Expr> initializer;
    string class_name;
    VarDecl(TokenType token_type, string n, bool is_arr, bool is_c,
            unique_ptr<Expr> size, unique_ptr<Expr> init, string cls = "")
        : type(token_type), name(move(n)), is_array(is_arr), is_const(is_c),
          array_size(move(size)), initializer(move(init)), class_name(move(cls)) {}
};

struct AssignStmt : Stmt {
    unique_ptr<Expr> target;
    unique_ptr<Expr> value;
    AssignStmt(unique_ptr<Expr> token_type, unique_ptr<Expr> value) 
        : target(move(token_type)), value(move(value)) {}
};

struct PrintStmt : Stmt {
    unique_ptr<Expr> expression;
    PrintStmt(unique_ptr<Expr> expr) 
        : expression(move(expr)) {}
};

struct BlockStmt : Stmt {
    vector<unique_ptr<Stmt>> statements;
    BlockStmt(vector<unique_ptr<Stmt>> s) 
        : statements(move(s)) {}
};

struct IfStmt : Stmt {
    unique_ptr<Expr> condition;
    unique_ptr<Stmt> then_branch;
    unique_ptr<Stmt> else_branch;
    IfStmt(unique_ptr<Expr> cond, unique_ptr<Stmt> then_b, unique_ptr<Stmt> else_b)
        : condition(move(cond)), then_branch(move(then_b)), else_branch(move(else_b)) {}
};

struct WhileStmt : Stmt {
    unique_ptr<Expr> condition;
    unique_ptr<Stmt> body;

    WhileStmt(unique_ptr<Expr> cond, unique_ptr<Stmt> b)
        : condition(move(cond)), body(move(b)) {}
};

struct ForStmt : Stmt {
    unique_ptr<Stmt> initializer;
    unique_ptr<Expr> condition;
    unique_ptr<Expr> increment;
    unique_ptr<Stmt> body;

    ForStmt(unique_ptr<Stmt> init, unique_ptr<Expr> cond, unique_ptr<Expr> inc, unique_ptr<Stmt> b)
        : initializer(move(init)), condition(move(cond)), increment(move(inc)), body(move(b)) {}
};

struct Param {
    TokenType type;
    string name;
    bool is_ref = false;
    Param(TokenType token_type, string n, bool r = false) : type(token_type), name(move(n)), is_ref(r) {}
};

struct ClassDecl : Stmt {
    string name;

    vector<unique_ptr<Stmt>> public_members;
    vector<unique_ptr<Stmt>> private_members;
    vector<unique_ptr<Stmt>> protected_members;

    ClassDecl(string n) 
        : name(move(n)) {}
};

struct FuncDecl : Stmt {
    TokenType return_type;
    string name;
    vector<Param> params;
    vector<unique_ptr<Stmt>> body;
    FuncDecl(TokenType rt, string n, vector<Param> p, vector<unique_ptr<Stmt>> b) 
        : return_type(rt), name(move(n)), params(move(p)), body(move(b)) {}
};

struct ReturnStmt : Stmt {
    unique_ptr<Expr> value;
    ReturnStmt(unique_ptr<Expr> value) 
        : value(move(value)) {}
};