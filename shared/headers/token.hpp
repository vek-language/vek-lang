#pragma once

#include <string>
#include <cstddef>
#include <stdexcept>

using std::size_t;
using std::string;
using std::runtime_error;

struct SourceLocation {
    size_t offset;
    size_t line;
};

enum class Type { INT32, INT64, FLOAT32, FLOAT64, BOOL, CHAR, STR, VOID, ARRAY, STRUCT, CLASS, UNKNOWN };
enum class TokenType {
    // Basic
    LOAD, CLASS, STRUCT, PUBLIC, PRIVATE, PROTECTED, THIS,
    
    LET, REF, CONST, FUNC, 
    IF, ELSE, WHILE, FOR,    
    PRINT, INPUT, RET,
    
    BREAK, CONTINUE,
    
    TYPEOF,
    // Declaration types
    INT32, INT64, FLOAT32, FLOAT64, 
    VOID, STR, CHAR, BOOL, IDENTIFIER,

    // Variable types
    INT32_LITERAL, INT64_LITERAL, FLOAT32_LITERAL, FLOAT64_LITERAL, 
    NIL, STR_LITERAL, CHAR_LITERAL, BOOL_LITERAL,

    // Math
    EQUALS, PLUS, MINUS, STAR, SLASH,
    PLUS_EQUALS, MINUS_EQUALS, STAR_EQUALS, SLASH_EQUALS,
    
    // Logic
    AND, OR, NOT, 
    GREATER_THAN, LESS_THAN, 
    GREATER_EQUAL, LESS_EQUAL, 
    EQUALS_EQUALS, NOT_EQUALS, 

    // Parens & Braces
    LPAREN, RPAREN, LBRACE, RBRACE, LBRACKET, RBRACKET, DOT,
    
    // Syntax
    SEMICOLON, COLON_COLON, COLON, COMMA,
};

struct Token {
    TokenType type;
    string text;
    SourceLocation location;
};

[[noreturn]] // For compiler
inline void report_error(const SourceLocation& loc, const std::string& message) {
    throw runtime_error(
        "[Line " + std::to_string(loc.line) + "] " + message
    );
}

string type_to_string(Type t) { 
    switch(t) {
        case Type::INT32:   return "int32"; 
        case Type::INT64:   return "int64";
        case Type::FLOAT32: return "float32"; 
        case Type::FLOAT64: return "float64";
        case Type::BOOL:    return "bool"; 
        case Type::CHAR:    return "char";
        case Type::STR:     return "str"; 
        case Type::VOID:    return "void";
        case Type::ARRAY:   return "array";
        case Type::STRUCT:  return "struct";
        case Type::CLASS:   return "class";
        default:            return "unknown";
    }
}