#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

struct Lexer {
    const char *src;
    size_t      pos;
    int         line, col;
    const char *filename;
    Token      *peeked;
};

static const struct { const char *kw; TokenType t; } KEYWORDS[] = {
    {"auto",TOK_AUTO},{"break",TOK_BREAK},{"case",TOK_CASE},
    {"char",TOK_CHAR_KW},{"const",TOK_CONST},{"continue",TOK_CONTINUE},
    {"default",TOK_DEFAULT},{"do",TOK_DO},{"double",TOK_DOUBLE},
    {"else",TOK_ELSE},{"enum",TOK_ENUM},{"extern",TOK_EXTERN},
    {"float",TOK_FLOAT_KW},{"for",TOK_FOR},{"goto",TOK_GOTO},
    {"if",TOK_IF},{"inline",TOK_INLINE},{"int",TOK_INT_KW},
    {"long",TOK_LONG},{"register",TOK_REGISTER},{"restrict",TOK_RESTRICT},
    {"return",TOK_RETURN},{"short",TOK_SHORT},{"signed",TOK_SIGNED},
    {"sizeof",TOK_SIZEOF},{"static",TOK_STATIC},{"struct",TOK_STRUCT},
    {"switch",TOK_SWITCH},{"typedef",TOK_TYPEDEF},{"union",TOK_UNION},
    {"unsigned",TOK_UNSIGNED},{"void",TOK_VOID},{"volatile",TOK_VOLATILE},
    {"while",TOK_WHILE},{"_Bool",TOK_BOOL_KW},{"NULL",TOK_NULL_KW},
    {NULL, TOK_EOF}
};

Lexer *lexer_create(const char *src, const char *filename) {
    Lexer *l = calloc(1, sizeof(Lexer));
    l->src      = src;
    l->line     = 1;
    l->col      = 1;
    l->filename = filename;
    return l;
}

void lexer_free(Lexer *l) {
    if (l->peeked) token_free(l->peeked);
    free(l);
}

void token_free(Token *t) {
    if (!t) return;
    free(t->value);
    free(t);
}

static Token *make_tok(TokenType type, const char *val,
                       int line, int col) {
    Token *t   = calloc(1, sizeof(Token));
    t->type    = type;
    t->value   = val ? strdup(val) : NULL;
    t->line    = line;
    t->col     = col;
    return t;
}

static char cur(Lexer *l)  { return l->src[l->pos]; }
static char peek1(Lexer *l){ return l->src[l->pos+1]; }

static char adv(Lexer *l) {
    char c = l->src[l->pos++];
    if (c == '\n') { l->line++; l->col = 1; }
    else            l->col++;
    return c;
}

static void skip_ws_comments(Lexer *l) {
    for (;;) {
        while (isspace((unsigned char)cur(l))) adv(l);
        if (cur(l) == '/' && peek1(l) == '/') {
            while (cur(l) && cur(l) != '\n') adv(l);
        } else if (cur(l) == '/' && peek1(l) == '*') {
            adv(l); adv(l);
            while (cur(l) && !(cur(l)=='*' && peek1(l)=='/')) adv(l);
            if (cur(l)) { adv(l); adv(l); }
        } else if (cur(l) == '#') {
            /* skip preprocessor lines (we rely on system cpp) */
            while (cur(l) && cur(l) != '\n') adv(l);
        } else break;
    }
}

static Token *lex_string(Lexer *l) {
    int ln = l->line, co = l->col;
    adv(l); /* skip " */
    char buf[4096]; int bi = 0;
    while (cur(l) && cur(l) != '"') {
        if (cur(l) == '\\') {
            adv(l);
            switch (cur(l)) {
                case 'n':  buf[bi++]='\n'; break;
                case 't':  buf[bi++]='\t'; break;
                case 'r':  buf[bi++]='\r'; break;
                case '0':  buf[bi++]='\0'; break;
                case '\\': buf[bi++]='\\'; break;
                case '"':  buf[bi++]='"';  break;
                default:   buf[bi++]=cur(l); break;
            }
            adv(l);
        } else buf[bi++] = adv(l);
    }
    if (cur(l) == '"') adv(l);
    buf[bi] = '\0';
    Token *t = make_tok(TOK_STR_LIT, buf, ln, co);
    return t;
}

static Token *lex_char(Lexer *l) {
    int ln = l->line, co = l->col;
    adv(l); /* skip ' */
    char c;
    if (cur(l) == '\\') {
        adv(l);
        switch (cur(l)) {
            case 'n':  c='\n'; break; case 't': c='\t'; break;
            case 'r':  c='\r'; break; case '0': c='\0'; break;
            case '\\': c='\\'; break; case '\'':c='\''; break;
            default:   c=cur(l); break;
        }
        adv(l);
    } else c = adv(l);
    if (cur(l) == '\'') adv(l);
    char buf[4]; snprintf(buf, sizeof(buf), "%c", c);
    Token *t = make_tok(TOK_CHAR_LIT, buf, ln, co);
    t->int_val = (unsigned char)c;
    return t;
}

static Token *lex_number(Lexer *l) {
    int ln = l->line, co = l->col;
    char buf[64]; int bi = 0;
    int is_float = 0;
    int base = 10;

    if (cur(l)=='0' && (peek1(l)=='x'||peek1(l)=='X')) {
        buf[bi++]=adv(l); buf[bi++]=adv(l); base=16;
        while (isxdigit((unsigned char)cur(l))) buf[bi++]=adv(l);
    } else if (cur(l)=='0' && (peek1(l)=='b'||peek1(l)=='B')) {
        adv(l); adv(l); base=2;
        while (cur(l)=='0'||cur(l)=='1') buf[bi++]=adv(l);
    } else {
        while (isdigit((unsigned char)cur(l))) buf[bi++]=adv(l);
        if (cur(l)=='.' && isdigit((unsigned char)peek1(l))) {
            is_float=1; buf[bi++]=adv(l);
            while (isdigit((unsigned char)cur(l))) buf[bi++]=adv(l);
        }
        if (cur(l)=='e'||cur(l)=='E') {
            is_float=1; buf[bi++]=adv(l);
            if (cur(l)=='+'||cur(l)=='-') buf[bi++]=adv(l);
            while (isdigit((unsigned char)cur(l))) buf[bi++]=adv(l);
        }
    }
    /* suffix: u, l, f, ul, ll, etc. */
    while (cur(l)=='u'||cur(l)=='U'||cur(l)=='l'||cur(l)=='L'||
           cur(l)=='f'||cur(l)=='F') {
        char s=adv(l);
        if (s=='f'||s=='F') is_float=1;
    }
    buf[bi]='\0';

    Token *t = make_tok(is_float ? TOK_FLOAT_LIT : TOK_INT_LIT, buf, ln, co);
    if (is_float)  t->float_val = strtod(buf, NULL);
    else           t->int_val   = strtoll(buf, NULL, base);
    return t;
}

Token *lexer_next(Lexer *l) {
    if (l->peeked) {
        Token *t = l->peeked;
        l->peeked = NULL;
        return t;
    }
    skip_ws_comments(l);
    int ln = l->line, co = l->col;

    if (!cur(l)) return make_tok(TOK_EOF, "", ln, co);

    /* String/char literals */
    if (cur(l) == '"')  return lex_string(l);
    if (cur(l) == '\'') return lex_char(l);

    /* Numbers */
    if (isdigit((unsigned char)cur(l)) ||
        (cur(l)=='.' && isdigit((unsigned char)peek1(l))))
        return lex_number(l);

    /* Identifiers & keywords */
    if (isalpha((unsigned char)cur(l)) || cur(l)=='_') {
        char buf[256]; int bi=0;
        while (isalnum((unsigned char)cur(l))||cur(l)=='_')
            buf[bi++]=adv(l);
        buf[bi]='\0';
        for (int i=0; KEYWORDS[i].kw; i++)
            if (!strcmp(buf, KEYWORDS[i].kw))
                return make_tok(KEYWORDS[i].t, buf, ln, co);
        return make_tok(TOK_IDENT, buf, ln, co);
    }

#define MATCH2(a,b,t2) if(cur(l)==a&&peek1(l)==b){adv(l);adv(l);return make_tok(t2,"",ln,co);}
#define MATCH1(a,t1)   if(cur(l)==a){adv(l);return make_tok(t1,"",ln,co);}

    /* Three-char */
    if(cur(l)=='<'&&peek1(l)=='<'&&l->src[l->pos+2]=='='){adv(l);adv(l);adv(l);return make_tok(TOK_LSHIFT_ASSIGN,"",ln,co);}
    if(cur(l)=='>'&&peek1(l)=='>'&&l->src[l->pos+2]=='='){adv(l);adv(l);adv(l);return make_tok(TOK_RSHIFT_ASSIGN,"",ln,co);}
    if(cur(l)=='.'&&peek1(l)=='.'&&l->src[l->pos+2]=='.'){adv(l);adv(l);adv(l);return make_tok(TOK_ELLIPSIS,"",ln,co);}

    MATCH2('+','+',TOK_INC)    MATCH2('-','-',TOK_DEC)
    MATCH2('=','=',TOK_EQ)     MATCH2('!','=',TOK_NEQ)
    MATCH2('<','=',TOK_LEQ)    MATCH2('>','=',TOK_GEQ)
    MATCH2('&','&',TOK_AND)    MATCH2('|','|',TOK_OR)
    MATCH2('<','<',TOK_LSHIFT) MATCH2('>','>',TOK_RSHIFT)
    MATCH2('+','=',TOK_PLUS_ASSIGN)  MATCH2('-','=',TOK_MINUS_ASSIGN)
    MATCH2('*','=',TOK_STAR_ASSIGN)  MATCH2('/','=',TOK_SLASH_ASSIGN)
    MATCH2('%','=',TOK_PERCENT_ASSIGN)
    MATCH2('&','=',TOK_AND_ASSIGN)   MATCH2('|','=',TOK_OR_ASSIGN)
    MATCH2('^','=',TOK_XOR_ASSIGN)
    MATCH2('-','>',TOK_ARROW)

    MATCH1('+',TOK_PLUS)    MATCH1('-',TOK_MINUS)
    MATCH1('*',TOK_STAR)    MATCH1('/',TOK_SLASH)
    MATCH1('%',TOK_PERCENT) MATCH1('=',TOK_ASSIGN)
    MATCH1('<',TOK_LT)      MATCH1('>',TOK_GT)
    MATCH1('!',TOK_NOT)     MATCH1('&',TOK_BAND)
    MATCH1('|',TOK_BOR)     MATCH1('^',TOK_BXOR)
    MATCH1('~',TOK_BNOT)    MATCH1('?',TOK_QUESTION)
    MATCH1(':',TOK_COLON)   MATCH1(',',TOK_COMMA)
    MATCH1('.',TOK_DOT)     MATCH1(';',TOK_SEMICOLON)
    MATCH1('(',TOK_LPAREN)  MATCH1(')',TOK_RPAREN)
    MATCH1('[',TOK_LBRACKET)MATCH1(']',TOK_RBRACKET)
    MATCH1('{',TOK_LBRACE)  MATCH1('}',TOK_RBRACE)
    MATCH1('#',TOK_HASH)

    char bad[3]={cur(l),'\0'};
    adv(l);
    return make_tok(TOK_ERROR, bad, ln, co);
}

Token *lexer_peek(Lexer *l) {
    if (!l->peeked) l->peeked = lexer_next(l);
    return l->peeked;
}

const char *token_type_str(TokenType t) {
    switch(t) {
#define C(x) case x: return #x;
        C(TOK_INT_LIT) C(TOK_FLOAT_LIT) C(TOK_CHAR_LIT) C(TOK_STR_LIT)
        C(TOK_IDENT) C(TOK_INT_KW) C(TOK_CHAR_KW) C(TOK_FLOAT_KW)
        C(TOK_VOID) C(TOK_RETURN) C(TOK_IF) C(TOK_ELSE) C(TOK_WHILE)
        C(TOK_FOR) C(TOK_DO) C(TOK_STRUCT) C(TOK_TYPEDEF) C(TOK_EXTERN)
        C(TOK_STATIC) C(TOK_CONST) C(TOK_UNSIGNED) C(TOK_SIGNED)
        C(TOK_LONG) C(TOK_SHORT) C(TOK_DOUBLE) C(TOK_SIZEOF)
        C(TOK_PLUS) C(TOK_MINUS) C(TOK_STAR) C(TOK_SLASH) C(TOK_ASSIGN)
        C(TOK_EQ) C(TOK_NEQ) C(TOK_LT) C(TOK_GT) C(TOK_LEQ) C(TOK_GEQ)
        C(TOK_AND) C(TOK_OR) C(TOK_NOT) C(TOK_INC) C(TOK_DEC)
        C(TOK_LPAREN) C(TOK_RPAREN) C(TOK_LBRACE) C(TOK_RBRACE)
        C(TOK_SEMICOLON) C(TOK_COMMA) C(TOK_EOF) C(TOK_ERROR)
#undef C
        default: return "TOK_?";
    }
}
