#ifndef VUCA_LEXER_H
#define VUCA_LEXER_H

#include <stddef.h>

typedef enum {
    /* Literals */
    TOK_INT_LIT, TOK_FLOAT_LIT, TOK_CHAR_LIT, TOK_STR_LIT,
    /* Identifier */
    TOK_IDENT,
    /* Keywords */
    TOK_AUTO,    TOK_BREAK,    TOK_CASE,    TOK_CHAR_KW, TOK_CONST,
    TOK_CONTINUE,TOK_DEFAULT,  TOK_DO,      TOK_DOUBLE,  TOK_ELSE,
    TOK_ENUM,    TOK_EXTERN,   TOK_FLOAT_KW,TOK_FOR,     TOK_GOTO,
    TOK_IF,      TOK_INLINE,   TOK_INT_KW,  TOK_LONG,    TOK_REGISTER,
    TOK_RESTRICT,TOK_RETURN,   TOK_SHORT,   TOK_SIGNED,  TOK_SIZEOF,
    TOK_STATIC,  TOK_STRUCT,   TOK_SWITCH,  TOK_TYPEDEF, TOK_UNION,
    TOK_UNSIGNED,TOK_VOID,     TOK_VOLATILE,TOK_WHILE,   TOK_BOOL_KW,
    TOK_NULL_KW,
    /* Operators */
    TOK_PLUS,TOK_MINUS,TOK_STAR,TOK_SLASH,TOK_PERCENT,
    TOK_ASSIGN,TOK_EQ,TOK_NEQ,TOK_LT,TOK_GT,TOK_LEQ,TOK_GEQ,
    TOK_AND,TOK_OR,TOK_NOT,
    TOK_BAND,TOK_BOR,TOK_BXOR,TOK_BNOT,TOK_LSHIFT,TOK_RSHIFT,
    TOK_INC,TOK_DEC,
    TOK_PLUS_ASSIGN,TOK_MINUS_ASSIGN,TOK_STAR_ASSIGN,
    TOK_SLASH_ASSIGN,TOK_PERCENT_ASSIGN,
    TOK_AND_ASSIGN,TOK_OR_ASSIGN,TOK_XOR_ASSIGN,
    TOK_LSHIFT_ASSIGN,TOK_RSHIFT_ASSIGN,
    TOK_QUESTION,TOK_COLON,TOK_COMMA,TOK_DOT,TOK_ARROW,TOK_ELLIPSIS,
    /* Delimiters */
    TOK_LPAREN,TOK_RPAREN,TOK_LBRACKET,TOK_RBRACKET,
    TOK_LBRACE,TOK_RBRACE,TOK_SEMICOLON,TOK_HASH,
    /* Special */
    TOK_EOF, TOK_ERROR
} TokenType;

typedef struct {
    TokenType   type;
    char       *value;       /* raw text                 */
    long long   int_val;
    double      float_val;
    int         line, col;
} Token;

typedef struct Lexer Lexer;

Lexer  *lexer_create(const char *src, const char *filename);
void    lexer_free(Lexer *l);
Token  *lexer_next(Lexer *l);
Token  *lexer_peek(Lexer *l);
void    token_free(Token *t);
const char *token_type_str(TokenType t);

#endif /* VUCA_LEXER_H */
