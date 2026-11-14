#pragma once

#include <iostream>
#include <vector>
#include <cctype>
#include <algorithm>
#include <unordered_map>
#include <string>

#include "token.hpp"

using std::runtime_error;
using std::unordered_map;
using std::vector;
using std::string;

inline const unordered_map<std::string, TokenType> keywords = {
    {"load", TokenType::LOAD},
    
    {"class", TokenType::CLASS},
    {"struct", TokenType::STRUCT},

    {"this", TokenType::THIS},
    {"public", TokenType::PUBLIC},
    {"private", TokenType::PRIVATE},
    {"protected", TokenType::PROTECTED},

    {"let", TokenType::LET},
    {"ref", TokenType::REF},
    {"const", TokenType::CONST},

    {"func", TokenType::FUNC},
    {"ret", TokenType::RET},
    
    {"if", TokenType::IF}, 
    {"else", TokenType::ELSE},
    {"while", TokenType::WHILE},
    {"for", TokenType::FOR},

    {"print", TokenType::PRINT},

    {"int32", TokenType::INT32},
    {"int64", TokenType::INT64},
    {"int", TokenType::INT64},

    {"float32", TokenType::FLOAT32},
    {"float64", TokenType::FLOAT64},
    {"float", TokenType::FLOAT64},
    
    {"str", TokenType::STR},
    {"char", TokenType::CHAR},
    {"bool", TokenType::BOOL},
    {"void", TokenType::VOID},

    {"not", TokenType::NOT},
    {"false", TokenType::BOOL_LITERAL},
    {"true", TokenType::BOOL_LITERAL},
    {"null", TokenType::NIL},
};

class Lexer {
    const string source;
    vector<Token> tokens;

    size_t start = 0;
    size_t current = 0;

    // Helpers
    bool is_at_end() const { return current >= source.size(); }

    void skip_whitespace() {
        while(!is_at_end()) {
            unsigned char c = static_cast<unsigned char>(peek());
            
            if (std::isspace(c) || c < 32 || c > 126) {
                ++current;
            } else {
                break;
            }
        }
    }

    void identifier() {
        while(!is_at_end()) {
            const unsigned char c = static_cast<unsigned char>(peek());
            if (!std::isalnum(c) && c != '_') break;
            ++current;
        }
    }

    void number() {
        while(!is_at_end() && std::isdigit(static_cast<unsigned char>(peek()))) {
            ++current;
        }
        if (!is_at_end() && peek() == '.') {
            if (current + 1 < source.size() && std::isdigit(static_cast<unsigned char>(source[current + 1]))) {
                ++current;
                while(!is_at_end() && std::isdigit(static_cast<unsigned char>(peek()))) {
                    ++current;
                }
            }
        }
    }

    Token make_token(TokenType type) const {
        return Token{
            .type = type,
            .text = source.substr(start, current - start),
            .location = SourceLocation{.offset = start}
        };
    }

    char decode_escape(char c) {
        switch (c) {
            case 'n':  return '\n'; case 't':  return '\t';
            case 'r':  return '\r'; case '\\': return '\\';
            case '"':  return '"';  case '\'': return '\'';
            case '0':  return '\0'; default:   return c;
        }
    }

    string scan_quoted(char quote) {
        string value;
        while (!is_at_end() && peek() != quote) {
            if (peek() == '\\') {
                next();
                if (is_at_end()) throw runtime_error("Unterminated escape sequence");
                value += decode_escape(next());
            } else {
                value += next();
            }
        }
        if (is_at_end()) throw runtime_error("Unterminated literal");
        next();
        return value;
    }

    Token scan_char(char quote) {
        std::string v = scan_quoted(quote);
        if (v.size() != 1) throw runtime_error("Char literal must contain exactly one character");
        Token token = make_token(TokenType::CHAR_LITERAL);
        token.text = std::move(v);
        return token;
    }

    Token scan_string(char quote) {
        Token token = make_token(TokenType::STR_LITERAL);
        token.text = scan_quoted(quote);
        return token;
    }

    char peek() const {
        if(is_at_end()) { return '\0'; }
        return source[current];
    }

    char next() {
        if(is_at_end()) { return '\0'; }
        return source[current++];
    }
public:
    explicit Lexer(const string& src) : source(src) {}

    vector<Token> lex() {
        while(!is_at_end()) {
            skip_whitespace();
            if(is_at_end()) break;

            start = current;
            const char c = next();

            switch(c) {
                case '"': tokens.push_back(scan_string(c)); break;
                case '\'': tokens.push_back(scan_char(c)); break;
                case ',': tokens.push_back(make_token(TokenType::COMMA)); break;
                case '.': tokens.push_back(make_token(TokenType::DOT)); break;
                case '=': 
                    if (peek() == '=') {
                        next();
                        tokens.push_back(make_token(TokenType::EQUALS_EQUALS));
                    } else {
                        tokens.push_back(make_token(TokenType::EQUALS));
                    }
                    break;

                case '&':
                    if (peek() == '&') {
                        next();
                        tokens.push_back(make_token(TokenType::AND));
                    } else {
                        throw runtime_error("Unexpected character '&'");
                    }
                    break;
                case '|':
                    if (peek() == '|') {
                        next();
                        tokens.push_back(make_token(TokenType::OR));
                    } else {
                        throw runtime_error("Unexpected character '|'");
                    }
                    break;

                case ';': tokens.push_back(make_token(TokenType::SEMICOLON)); break;
                case '{': tokens.push_back(make_token(TokenType::LBRACE)); break;
                case '}': tokens.push_back(make_token(TokenType::RBRACE)); break;
                case '[': tokens.push_back(make_token(TokenType::LBRACKET)); break;
                case ']': tokens.push_back(make_token(TokenType::RBRACKET)); break;
                    
                case ':':
                    if (peek() == ':') { 
                        next(); 
                        tokens.push_back(make_token(TokenType::COLON_COLON)); 
                    } else { 
                        tokens.push_back(make_token(TokenType::COLON));
                    }
                    break;
                
                case '+':
                    if (peek() == '=') {
                        next();
                        tokens.push_back(make_token(TokenType::PLUS_EQUALS));
                    } else {
                        tokens.push_back(make_token(TokenType::PLUS));
                    }
                    break;
                    
                case '-':                     
                    if (peek() == '=') {
                        next();
                        tokens.push_back(make_token(TokenType::MINUS_EQUALS));
                    } else {
                        tokens.push_back(make_token(TokenType::MINUS));
                    }
                    break;
                    
                case '*':                     
                    if (peek() == '=') {
                        next();
                        tokens.push_back(make_token(TokenType::STAR_EQUALS));
                    } else {
                        tokens.push_back(make_token(TokenType::STAR));
                    }
                    break;
                    
                case '/':
                    if (peek() == '/') { 
                        // Comment
                        while(peek() != '\n' && !is_at_end()) next(); 
                    } 
                    else if (peek() == '=') { 
                        next(); 
                        tokens.push_back(make_token(TokenType::SLASH_EQUALS)); 
                    } 
                    else { 
                        tokens.push_back(make_token(TokenType::SLASH)); 
                    }
                    break;

                case '#':
                    while(peek() != '\n' && !is_at_end()) next();
                    break;

                case '(': tokens.push_back(make_token(TokenType::LPAREN)); break;
                case ')': tokens.push_back(make_token(TokenType::RPAREN)); break;
                case '!':
                    if (peek() == '=') {
                        next();
                        tokens.push_back(make_token(TokenType::NOT_EQUALS));
                    } else {
                        tokens.push_back(make_token(TokenType::NOT));
                    }
                break;
                
                case '>':
                    if (peek() == '=') { next(); tokens.push_back(make_token(TokenType::GREATER_EQUAL)); }
                    else { tokens.push_back(make_token(TokenType::GREATER_THAN)); }
                    break;
                case '<':
                    if (peek() == '=') { next(); tokens.push_back(make_token(TokenType::LESS_EQUAL)); }
                    else { tokens.push_back(make_token(TokenType::LESS_THAN)); }
                    break;

                default:
                    if(std::isalpha(c) || c == '_') {
                        identifier();
                        Token tok = make_token(TokenType::IDENTIFIER);
                        string lower = tok.text;
                        for (auto& ch : lower) ch = static_cast<char>(std::tolower((unsigned char)ch));
                        auto it = keywords.find(lower);
                        if (it != keywords.end()) tok.type = it->second;
                        tokens.push_back(tok);
                    } else if(std::isdigit(c)) {
                        number();
                        
                        bool is_f32 = false;
                        bool is_f64 = false;

                        // d or f, for explicit type too
                        if (peek() == 'f' || peek() == 'F') {
                            next();
                            is_f32 = true;
                        } else if (peek() == 'd' || peek() == 'D') {
                            next();
                            is_f64 = true;
                        }

                        if (is_f32) {
                            tokens.push_back(make_token(TokenType::FLOAT32_LITERAL));
                        } else if (is_f64) {
                            tokens.push_back(make_token(TokenType::FLOAT64_LITERAL));
                        } else if (source.substr(start, current - start).find('.') != string::npos) {
                            tokens.push_back(make_token(TokenType::FLOAT64_LITERAL));
                        } else {
                            tokens.push_back(make_token(TokenType::INT64_LITERAL));
                        }
                    } else { 
                        std::cout << "Unknown character: '" << c << "' (ASCII: " << (int)c << ")\n";
                        throw runtime_error("Unexpected character."); 
                    }
                    break;
            }
        }
        return tokens;
    }
};