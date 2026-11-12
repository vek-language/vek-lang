#pragma once

#include <string>
#include <cstddef>

using std::size_t;
using std::string;

struct SourceLocation {
    size_t offset;
};

enum class TokenType {
    // Basic
    LOAD, CLASS, PUBLIC, PRIVATE, PROTECTED, THIS,
    
    LET, REF, CONST, FUNC, 
    IF, ELSE, WHILE, FOR,    
    PRINT, RET,
    
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
    SEMICOLON, COLON_COLON, COMMA,
};

struct Token {
    TokenType type;
    string text;
    SourceLocation location;
};