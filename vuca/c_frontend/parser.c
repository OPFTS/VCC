#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── helpers ────────────────────────────────────────────────── */

Parser *parser_create(Lexer *lex, const char *filename) {
    Parser *p = calloc(1, sizeof(Parser));
    p->lex = lex;
    p->filename = filename;
    return p;
}

void parser_free(Parser *p) { free(p); }

static Token *peek(Parser *p)       { return lexer_peek(p->lex); }
static Token *advance(Parser *p)    { return lexer_next(p->lex); }
static int    check(Parser *p, TokenType t) { return peek(p)->type == t; }

static Token *expect(Parser *p, TokenType t) {
    Token *tok = peek(p);
    if (tok->type != t) {
        fprintf(stderr, "%s:%d:%d: error: expected %s, got '%s' (%s)\n",
                p->filename, tok->line, tok->col,
                token_type_str(t), tok->value ? tok->value : "",
                token_type_str(tok->type));
        p->errors++;
        return tok;
    }
    return advance(p);
}

static int is_type_start(Parser *p) {
    TokenType t = peek(p)->type;
    return t==TOK_INT_KW||t==TOK_CHAR_KW||t==TOK_FLOAT_KW||
           t==TOK_DOUBLE||t==TOK_VOID||t==TOK_SHORT||t==TOK_LONG||
           t==TOK_UNSIGNED||t==TOK_SIGNED||t==TOK_STRUCT||
           t==TOK_UNION||t==TOK_CONST||t==TOK_STATIC||t==TOK_EXTERN||
           t==TOK_TYPEDEF||t==TOK_BOOL_KW||t==TOK_INLINE||
           t==TOK_VOLATILE||t==TOK_RESTRICT;
}

/* ── type parsing ───────────────────────────────────────────── */

static Type *parse_base_type(Parser *p) {
    int is_unsigned = 0, is_signed = 0, is_const = 0, is_long = 0, is_short = 0;

    for (;;) {
        TokenType t = peek(p)->type;
        if (t==TOK_UNSIGNED)   { is_unsigned=1; advance(p); }
        else if (t==TOK_SIGNED){ is_signed=1;   advance(p); }
        else if (t==TOK_CONST) { is_const=1;    advance(p); }
        else if (t==TOK_LONG)  { is_long++;     advance(p); }
        else if (t==TOK_SHORT) { is_short=1;    advance(p); }
        else if (t==TOK_STATIC||t==TOK_EXTERN||t==TOK_INLINE||
                 t==TOK_VOLATILE||t==TOK_RESTRICT) advance(p);
        else break;
    }

    Type *base = NULL;
    TokenType t = peek(p)->type;

    if (t==TOK_VOID)     { advance(p); base=type_new(TY_VOID);  }
    else if (t==TOK_CHAR_KW){ advance(p); base=type_new(is_unsigned?TY_U8:TY_I8); }
    else if (t==TOK_FLOAT_KW){advance(p); base=type_new(TY_F32); }
    else if (t==TOK_DOUBLE){ advance(p); base=type_new(TY_F64);  }
    else if (t==TOK_BOOL_KW){ advance(p); base=type_new(TY_BOOL);}
    else if (t==TOK_INT_KW||is_long||is_short||is_unsigned||is_signed) {
        if (t==TOK_INT_KW) advance(p);
        if      (is_short)         base=type_new(is_unsigned?TY_U16:TY_I16);
        else if (is_long>=2)       base=type_new(is_unsigned?TY_U64:TY_I64);
        else if (is_long==1)       base=type_new(is_unsigned?TY_U64:TY_I64);
        else if (is_unsigned)      base=type_new(TY_U32);
        else                       base=type_new(TY_I32);
    } else if (t==TOK_STRUCT||t==TOK_UNION) {
        advance(p);
        base = type_new(t==TOK_STRUCT ? TY_STRUCT : TY_UNION);
        if (check(p, TOK_IDENT)) {
            Token *name = advance(p);
            base->name = strdup(name->value);
            token_free(name);
        }
        /* skip struct body for now (extern/prototype use) */
        if (check(p, TOK_LBRACE)) {
            int depth=1; advance(p);
            while (!check(p,TOK_EOF)&&depth>0) {
                if(check(p,TOK_LBRACE)) depth++;
                else if(check(p,TOK_RBRACE)) depth--;
                Token *tmp=advance(p); token_free(tmp);
            }
        }
    } else {
        /* unknown – treat as i32 and consume ident if present */
        if (check(p,TOK_IDENT)) {
            Token *name=advance(p);
            base=type_new(TY_I32);
            base->name=strdup(name->value);
            token_free(name);
        } else {
            base=type_new(TY_I32);
        }
    }

    base->is_const = is_const;

    /* pointer decorators */
    while (check(p,TOK_STAR)) {
        advance(p);
        int ptr_const=0;
        if(check(p,TOK_CONST)){advance(p);ptr_const=1;}
        base=type_ptr(base);
        base->is_const=ptr_const;
    }
    return base;
}

/* forward declarations */
static ASTNode *parse_expr(Parser *p);
static ASTNode *parse_stmt(Parser *p);
static ASTNode *parse_decl(Parser *p);

/* ── Expression parsing (recursive descent) ─────────────────── */

static ASTNode *make_binop(const char *op, ASTNode *l, ASTNode *r, int line) {
    ASTNode *n = ast_node_new();
    n->category = NODE_EXPR;
    n->expr.kind = EXPR_BINARY;
    n->expr.op   = strdup(op);
    n->expr.left  = l;
    n->expr.right = r;
    n->line = line;
    return n;
}

static ASTNode *parse_primary(Parser *p) {
    Token *tok = peek(p);
    ASTNode *n = ast_node_new();
    n->category = NODE_EXPR;
    n->line = tok->line;

    if (tok->type==TOK_INT_LIT) {
        tok=advance(p);
        n->expr.kind=EXPR_INT_LIT; n->expr.int_val=tok->int_val;
        token_free(tok); return n;
    }
    if (tok->type==TOK_FLOAT_LIT) {
        tok=advance(p);
        n->expr.kind=EXPR_FLOAT_LIT; n->expr.float_val=tok->float_val;
        token_free(tok); return n;
    }
    if (tok->type==TOK_STR_LIT) {
        tok=advance(p);
        n->expr.kind=EXPR_STR_LIT; n->expr.str_val=strdup(tok->value);
        token_free(tok); return n;
    }
    if (tok->type==TOK_CHAR_LIT) {
        tok=advance(p);
        n->expr.kind=EXPR_CHAR_LIT; n->expr.int_val=tok->int_val;
        token_free(tok); return n;
    }
    if (tok->type==TOK_NULL_KW) {
        advance(p); n->expr.kind=EXPR_NULL_LIT; return n;
    }
    if (tok->type==TOK_IDENT) {
        tok=advance(p);
        n->expr.kind=EXPR_IDENT; n->expr.ident=strdup(tok->value);
        token_free(tok); return n;
    }
    if (tok->type==TOK_LPAREN) {
        token_free(advance(p));
        /* cast or parenthesized expr */
        if (is_type_start(p)) {
            Type *cast_ty=parse_base_type(p);
            token_free(expect(p,TOK_RPAREN));
            ASTNode *sub=parse_primary(p);
            n->expr.kind=EXPR_CAST;
            n->expr.cast_type=cast_ty;
            n->expr.left=sub;
            return n;
        }
        free(n);
        ASTNode *inner=parse_expr(p);
        token_free(expect(p,TOK_RPAREN));
        return inner;
    }
    if (tok->type==TOK_SIZEOF) {
        advance(p);
        token_free(expect(p,TOK_LPAREN));
        if (is_type_start(p)) {
            Type *ty=parse_base_type(p);
            token_free(expect(p,TOK_RPAREN));
            n->expr.kind=EXPR_SIZEOF_TYPE;
            n->expr.cast_type=ty;
            return n;
        }
        ASTNode *sub=parse_expr(p);
        token_free(expect(p,TOK_RPAREN));
        n->expr.kind=EXPR_SIZEOF_EXPR;
        n->expr.left=sub;
        return n;
    }
    /* unary prefix */
    if (tok->type==TOK_MINUS||tok->type==TOK_NOT||tok->type==TOK_BNOT) {
        tok=advance(p);
        const char *op = tok->type==TOK_MINUS?"-":tok->type==TOK_NOT?"!":"~";
        token_free(tok);
        n->expr.kind=EXPR_UNARY;
        n->expr.op=strdup(op);
        n->expr.left=parse_primary(p);
        return n;
    }
    if (tok->type==TOK_STAR) {     /* dereference */
        advance(p);
        n->expr.kind=EXPR_DEREF;
        n->expr.left=parse_primary(p); return n;
    }
    if (tok->type==TOK_BAND) {     /* address-of */
        advance(p);
        n->expr.kind=EXPR_ADDR;
        n->expr.left=parse_primary(p); return n;
    }
    if (tok->type==TOK_INC) { advance(p); n->expr.kind=EXPR_INC_PRE;  n->expr.left=parse_primary(p); return n; }
    if (tok->type==TOK_DEC) { advance(p); n->expr.kind=EXPR_DEC_PRE;  n->expr.left=parse_primary(p); return n; }

    /* unknown */
    fprintf(stderr,"%s:%d: error: unexpected token '%s'\n",
            p->filename, tok->line, tok->value?tok->value:"?");
    p->errors++;
    free(n); return NULL;
}

static ASTNode *parse_postfix(Parser *p) {
    ASTNode *left = parse_primary(p);
    if (!left) return NULL;
    for (;;) {
        int ln = peek(p)->line;
        if (check(p,TOK_LPAREN)) {
            token_free(advance(p));
            /* function call */
            ASTNode *call=ast_node_new();
            call->category=NODE_EXPR;
            call->expr.kind=EXPR_CALL;
            call->expr.callee=left;
            call->line=ln;
            /* arguments */
            ASTNode **args=NULL; int ac=0;
            while (!check(p,TOK_RPAREN)&&!check(p,TOK_EOF)) {
                args=realloc(args,(ac+1)*sizeof(ASTNode*));
                args[ac++]=parse_expr(p);
                if (check(p,TOK_COMMA)) token_free(advance(p));
                else break;
            }
            token_free(expect(p,TOK_RPAREN));
            call->expr.args=args;
            call->expr.arg_count=ac;
            left=call;
        } else if (check(p,TOK_LBRACKET)) {
            token_free(advance(p));
            ASTNode *idx=parse_expr(p);
            token_free(expect(p,TOK_RBRACKET));
            ASTNode *n=ast_node_new();
            n->category=NODE_EXPR; n->expr.kind=EXPR_INDEX;
            n->expr.base_expr=left; n->expr.left=idx; n->line=ln;
            left=n;
        } else if (check(p,TOK_DOT)) {
            token_free(advance(p));
            Token *field=expect(p,TOK_IDENT);
            ASTNode *n=ast_node_new();
            n->category=NODE_EXPR; n->expr.kind=EXPR_MEMBER;
            n->expr.base_expr=left; n->expr.member=strdup(field->value);
            n->line=ln; token_free(field); left=n;
        } else if (check(p,TOK_ARROW)) {
            token_free(advance(p));
            Token *field=expect(p,TOK_IDENT);
            ASTNode *n=ast_node_new();
            n->category=NODE_EXPR; n->expr.kind=EXPR_MEMBER_PTR;
            n->expr.base_expr=left; n->expr.member=strdup(field->value);
            n->line=ln; token_free(field); left=n;
        } else if (check(p,TOK_INC)) {
            token_free(advance(p));
            ASTNode *n=ast_node_new();
            n->category=NODE_EXPR; n->expr.kind=EXPR_INC_POST;
            n->expr.left=left; n->line=ln; left=n;
        } else if (check(p,TOK_DEC)) {
            token_free(advance(p));
            ASTNode *n=ast_node_new();
            n->category=NODE_EXPR; n->expr.kind=EXPR_DEC_POST;
            n->expr.left=left; n->line=ln; left=n;
        } else break;
    }
    return left;
}

/* operator precedence table */
static int binop_prec(TokenType t) {
    switch(t) {
        case TOK_STAR: case TOK_SLASH: case TOK_PERCENT: return 12;
        case TOK_PLUS: case TOK_MINUS:                   return 11;
        case TOK_LSHIFT: case TOK_RSHIFT:                return 10;
        case TOK_LT: case TOK_GT: case TOK_LEQ: case TOK_GEQ: return 9;
        case TOK_EQ: case TOK_NEQ:                       return 8;
        case TOK_BAND:                                   return 7;
        case TOK_BXOR:                                   return 6;
        case TOK_BOR:                                    return 5;
        case TOK_AND:                                    return 4;
        case TOK_OR:                                     return 3;
        default:                                         return -1;
    }
}

static const char *binop_str(TokenType t) {
    switch(t) {
        case TOK_PLUS:    return "+";
        case TOK_MINUS:   return "-";
        case TOK_STAR:    return "*";
        case TOK_SLASH:   return "/";
        case TOK_PERCENT: return "%";
        case TOK_LT:      return "<";
        case TOK_GT:      return ">";
        case TOK_LEQ:     return "<=";
        case TOK_GEQ:     return ">=";
        case TOK_EQ:      return "==";
        case TOK_NEQ:     return "!=";
        case TOK_AND:     return "&&";
        case TOK_OR:      return "||";
        case TOK_BAND:    return "&";
        case TOK_BOR:     return "|";
        case TOK_BXOR:    return "^";
        case TOK_LSHIFT:  return "<<";
        case TOK_RSHIFT:  return ">>";
        default:          return "?";
    }
}

static ASTNode *parse_binop(Parser *p, int min_prec) {
    ASTNode *left = parse_postfix(p);
    if (!left) return NULL;
    for (;;) {
        Token *op_tok = peek(p);
        int prec = binop_prec(op_tok->type);
        if (prec < min_prec) break;
        int ln = op_tok->line;
        TokenType op_type = op_tok->type;
        token_free(advance(p));
        ASTNode *right = parse_binop(p, prec + 1);
        left = make_binop(binop_str(op_type), left, right, ln);
    }
    return left;
}

static ASTNode *parse_assign(Parser *p) {
    ASTNode *left = parse_binop(p, 0);
    if (!left) return NULL;
    Token *t = peek(p);
    const char *op = NULL;
    switch(t->type) {
        case TOK_ASSIGN:         op="=";   break;
        case TOK_PLUS_ASSIGN:    op="+=";  break;
        case TOK_MINUS_ASSIGN:   op="-=";  break;
        case TOK_STAR_ASSIGN:    op="*=";  break;
        case TOK_SLASH_ASSIGN:   op="/=";  break;
        case TOK_PERCENT_ASSIGN: op="%=";  break;
        case TOK_AND_ASSIGN:     op="&=";  break;
        case TOK_OR_ASSIGN:      op="|=";  break;
        case TOK_XOR_ASSIGN:     op="^=";  break;
        case TOK_LSHIFT_ASSIGN:  op="<<="; break;
        case TOK_RSHIFT_ASSIGN:  op=">>="; break;
        default: break;
    }
    if (op) {
        int ln=t->line; token_free(advance(p));
        ASTNode *right=parse_assign(p);
        ASTNode *n=ast_node_new();
        n->category=NODE_EXPR; n->line=ln;
        n->expr.kind=EXPR_ASSIGN;
        n->expr.op=strdup(op);
        n->expr.left=left; n->expr.right=right;
        return n;
    }
    /* ternary */
    if (peek(p)->type==TOK_QUESTION) {
        int ln=peek(p)->line; token_free(advance(p));
        ASTNode *then_e=parse_expr(p);
        token_free(expect(p,TOK_COLON));
        ASTNode *else_e=parse_assign(p);
        ASTNode *n=ast_node_new();
        n->category=NODE_EXPR; n->line=ln;
        n->expr.kind=EXPR_TERNARY;
        n->expr.cond=left; n->expr.left=then_e; n->expr.right=else_e;
        return n;
    }
    return left;
}

static ASTNode *parse_expr(Parser *p) {
    return parse_assign(p);
}

/* ── Statement parsing ──────────────────────────────────────── */

static ASTNode *parse_block(Parser *p) {
    ASTNode *n = ast_node_new();
    n->category = NODE_STMT;
    n->expr.kind = EXPR_NULL_LIT; /* unused */
    n->stmt.kind = STMT_BLOCK;
    n->line = peek(p)->line;
    token_free(expect(p, TOK_LBRACE));

    ASTNode **stmts = NULL; int sc = 0;
    while (!check(p,TOK_RBRACE) && !check(p,TOK_EOF)) {
        ASTNode *s = parse_stmt(p);
        if (s) {
            stmts = realloc(stmts, (sc+1)*sizeof(ASTNode*));
            stmts[sc++] = s;
        }
    }
    token_free(expect(p, TOK_RBRACE));
    n->stmt.stmts = stmts;
    n->stmt.stmt_count = sc;
    return n;
}

static ASTNode *parse_stmt(Parser *p) {
    Token *t = peek(p);
    ASTNode *n;

    /* declaration */
    if (is_type_start(p)) return parse_decl(p);

    n = ast_node_new();
    n->category = NODE_STMT;
    n->line = t->line;

    switch (t->type) {
    case TOK_LBRACE:
        free(n);
        return parse_block(p);

    case TOK_IF: {
        token_free(advance(p));
        n->stmt.kind = STMT_IF;
        token_free(expect(p,TOK_LPAREN));
        n->stmt.cond = parse_expr(p);
        token_free(expect(p,TOK_RPAREN));
        n->stmt.body = parse_stmt(p);
        if (check(p,TOK_ELSE)) {
            token_free(advance(p));
            n->stmt.else_body = parse_stmt(p);
        }
        return n;
    }
    case TOK_WHILE: {
        token_free(advance(p));
        n->stmt.kind = STMT_WHILE;
        token_free(expect(p,TOK_LPAREN));
        n->stmt.cond = parse_expr(p);
        token_free(expect(p,TOK_RPAREN));
        n->stmt.body = parse_stmt(p);
        return n;
    }
    case TOK_DO: {
        token_free(advance(p));
        n->stmt.kind = STMT_DO_WHILE;
        n->stmt.body = parse_stmt(p);
        token_free(expect(p,TOK_WHILE));
        token_free(expect(p,TOK_LPAREN));
        n->stmt.cond = parse_expr(p);
        token_free(expect(p,TOK_RPAREN));
        token_free(expect(p,TOK_SEMICOLON));
        return n;
    }
    case TOK_FOR: {
        token_free(advance(p));
        n->stmt.kind = STMT_FOR;
        token_free(expect(p,TOK_LPAREN));
        if (!check(p,TOK_SEMICOLON)) {
            if (is_type_start(p)) n->stmt.init = parse_decl(p);
            else { n->stmt.init=ast_node_new(); n->stmt.init->category=NODE_STMT;
                   n->stmt.init->stmt.kind=STMT_EXPR;
                   n->stmt.init->stmt.expr=parse_expr(p);
                   token_free(expect(p,TOK_SEMICOLON)); }
        } else token_free(advance(p));
        if (!check(p,TOK_SEMICOLON)) n->stmt.cond=parse_expr(p);
        token_free(expect(p,TOK_SEMICOLON));
        if (!check(p,TOK_RPAREN))   n->stmt.step=parse_expr(p);
        token_free(expect(p,TOK_RPAREN));
        n->stmt.body = parse_stmt(p);
        return n;
    }
    case TOK_RETURN: {
        token_free(advance(p));
        n->stmt.kind = STMT_RETURN;
        if (!check(p,TOK_SEMICOLON)) n->stmt.expr=parse_expr(p);
        token_free(expect(p,TOK_SEMICOLON));
        return n;
    }
    case TOK_BREAK:
        token_free(advance(p)); token_free(expect(p,TOK_SEMICOLON));
        n->stmt.kind=STMT_BREAK; return n;
    case TOK_CONTINUE:
        token_free(advance(p)); token_free(expect(p,TOK_SEMICOLON));
        n->stmt.kind=STMT_CONTINUE; return n;
    case TOK_GOTO: {
        token_free(advance(p));
        Token *lbl=expect(p,TOK_IDENT);
        n->stmt.kind=STMT_GOTO; n->stmt.label=strdup(lbl->value);
        token_free(lbl); token_free(expect(p,TOK_SEMICOLON)); return n;
    }
    case TOK_SEMICOLON:
        token_free(advance(p)); n->stmt.kind=STMT_NULL; return n;
    default: {
        /* expression statement or labeled statement */
        ASTNode *e = parse_expr(p);
        if (check(p,TOK_COLON) && e &&
            e->category==NODE_EXPR && e->expr.kind==EXPR_IDENT) {
            /* label */
            token_free(advance(p));
            n->stmt.kind=STMT_LABEL;
            n->stmt.label=strdup(e->expr.ident);
            n->stmt.body=parse_stmt(p);
            ast_node_free(e); return n;
        }
        token_free(expect(p,TOK_SEMICOLON));
        n->stmt.kind=STMT_EXPR; n->stmt.expr=e;
        return n;
    }
    }
}

/* ── Declaration parsing ────────────────────────────────────── */

static ASTNode *parse_decl(Parser *p) {
    ASTNode *n = ast_node_new();
    n->category = NODE_DECL;
    n->line = peek(p)->line;

    /* storage class */
    while (check(p,TOK_EXTERN)||check(p,TOK_STATIC)||
           check(p,TOK_TYPEDEF)||check(p,TOK_INLINE)) {
        TokenType tt=peek(p)->type;
        token_free(advance(p));
        if (tt==TOK_EXTERN)  n->decl.is_extern=1;
        if (tt==TOK_STATIC)  n->decl.is_static=1;
        if (tt==TOK_TYPEDEF) n->decl.is_typedef=1;
    }

    Type *base = parse_base_type(p);
    n->decl.type = base;

    /* name */
    if (check(p,TOK_IDENT)) {
        Token *name=advance(p);
        n->decl.name=strdup(name->value);
        token_free(name);
    }

    /* array suffix */
    if (check(p,TOK_LBRACKET)) {
        token_free(advance(p));
        long sz=-1;
        if (!check(p,TOK_RBRACKET)) {
            Token *sz_tok=advance(p);
            sz=sz_tok->int_val; token_free(sz_tok);
        }
        token_free(expect(p,TOK_RBRACKET));
        n->decl.type=type_array(base,sz);
    }

    /* function definition or prototype */
    if (check(p,TOK_LPAREN)) {
        token_free(advance(p));
        char  **pnames=NULL;
        Type  **ptypes=NULL;
        int     pc=0;
        while (!check(p,TOK_RPAREN)&&!check(p,TOK_EOF)) {
            if (check(p,TOK_ELLIPSIS)) {
                token_free(advance(p));
                n->decl.is_variadic=1; break;
            }
            if (is_type_start(p)) {
                Type *pt=parse_base_type(p);
                char *pn=NULL;
                if (check(p,TOK_IDENT)) {
                    Token *pname=advance(p);
                    pn=strdup(pname->value); token_free(pname);
                }
                ptypes=realloc(ptypes,(pc+1)*sizeof(Type*));
                pnames=realloc(pnames,(pc+1)*sizeof(char*));
                ptypes[pc]=pt; pnames[pc]=pn; pc++;
            }
            if (check(p,TOK_COMMA)) token_free(advance(p));
            else break;
        }
        token_free(expect(p,TOK_RPAREN));

        /* build function type */
        Type *fty=type_new(TY_FUNC);
        fty->base=base;
        fty->params=ptypes;
        fty->param_count=pc;
        fty->is_variadic=n->decl.is_variadic;
        n->decl.type=fty;
        n->decl.param_names=pnames;

        if (check(p,TOK_LBRACE)) {
            n->decl.body=parse_block(p);
        } else {
            token_free(expect(p,TOK_SEMICOLON));
        }
        return n;
    }

    /* variable init */
    if (check(p,TOK_ASSIGN)) {
        token_free(advance(p));
        n->decl.init=parse_expr(p);
    }
    token_free(expect(p,TOK_SEMICOLON));
    return n;
}

/* ── Translation unit ───────────────────────────────────────── */

TranslationUnit *parser_parse(Parser *p) {
    TranslationUnit *tu = tu_new();
    while (!check(p,TOK_EOF)) {
        ASTNode *d = parse_decl(p);
        if (!d) break;
        tu->decls = realloc(tu->decls, (tu->count+1)*sizeof(ASTNode*));
        tu->decls[tu->count++] = d;
        if (p->errors > 20) {
            fprintf(stderr,"%s: too many errors, aborting\n",p->filename);
            break;
        }
    }
    return tu;
}
