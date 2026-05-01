#ifndef VUCA_AST_H
#define VUCA_AST_H

#include <stddef.h>

/* ── Type system ───────────────────────────────────────────── */
typedef enum {
    TY_VOID, TY_BOOL,
    TY_I8, TY_I16, TY_I32, TY_I64,
    TY_U8, TY_U16, TY_U32, TY_U64,
    TY_F32, TY_F64,
    TY_PTR, TY_ARRAY, TY_STRUCT, TY_UNION,
    TY_FUNC, TY_TYPEDEF_REF
} TypeKind;

typedef struct Type Type;
typedef struct ASTNode ASTNode;

struct Type {
    TypeKind    kind;
    Type       *base;          /* for PTR, ARRAY, FUNC return */
    long        array_size;    /* -1 = incomplete              */
    char       *name;          /* struct/union/typedef name    */
    Type      **params;        /* FUNC param types             */
    int         param_count;
    int         is_const;
    int         is_volatile;
    int         is_variadic;
};

/* ── Expression kinds ──────────────────────────────────────── */
typedef enum {
    EXPR_INT_LIT, EXPR_FLOAT_LIT, EXPR_STR_LIT, EXPR_CHAR_LIT,
    EXPR_IDENT, EXPR_CALL, EXPR_INDEX,
    EXPR_MEMBER, EXPR_MEMBER_PTR,
    EXPR_UNARY, EXPR_BINARY, EXPR_TERNARY,
    EXPR_ASSIGN, EXPR_COMPOUND_ASSIGN,
    EXPR_CAST, EXPR_SIZEOF_EXPR, EXPR_SIZEOF_TYPE,
    EXPR_ADDR, EXPR_DEREF,
    EXPR_INC_PRE, EXPR_DEC_PRE,
    EXPR_INC_POST, EXPR_DEC_POST,
    EXPR_COMMA_LIST,
    EXPR_NULL_LIT
} ExprKind;

/* ── Statement kinds ──────────────────────────────────────── */
typedef enum {
    STMT_EXPR, STMT_BLOCK, STMT_IF, STMT_WHILE, STMT_DO_WHILE,
    STMT_FOR, STMT_RETURN, STMT_BREAK, STMT_CONTINUE,
    STMT_DECL, STMT_GOTO, STMT_LABEL, STMT_SWITCH, STMT_CASE,
    STMT_DEFAULT, STMT_NULL
} StmtKind;

/* ── AST Node ─────────────────────────────────────────────── */
struct ASTNode {
    enum { NODE_EXPR, NODE_STMT, NODE_DECL } category;
    union {
        /* Expression */
        struct {
            ExprKind   kind;
            Type      *type;
            long long  int_val;
            double     float_val;
            char      *str_val;
            char      *ident;
            char      *op;           /* binary/unary operator    */
            ASTNode   *left;
            ASTNode   *right;
            ASTNode   *cond;         /* ternary condition        */
            ASTNode  **args;         /* call arguments           */
            int        arg_count;
            ASTNode   *callee;
            Type      *cast_type;
            ASTNode   *base_expr;    /* for index/member         */
            char      *member;
        } expr;
        /* Statement */
        struct {
            StmtKind   kind;
            ASTNode   *expr;
            ASTNode  **stmts;        /* block body               */
            int        stmt_count;
            ASTNode   *init;         /* for-init                 */
            ASTNode   *cond;
            ASTNode   *step;
            ASTNode   *body;
            ASTNode   *else_body;
            char      *label;
        } stmt;
        /* Declaration */
        struct {
            Type      *type;
            char      *name;
            ASTNode   *init;
            int        is_extern;
            int        is_static;
            int        is_typedef;
            /* Function */
            char     **param_names;
            ASTNode   *body;         /* NULL = prototype         */
            int        is_variadic;
        } decl;
    };
    int line;
};

/* ── Translation unit ─────────────────────────────────────── */
typedef struct {
    ASTNode **decls;
    int       count;
} TranslationUnit;

ASTNode        *ast_node_new(void);
void            ast_node_free(ASTNode *n);
TranslationUnit*tu_new(void);
void            tu_free(TranslationUnit *tu);
Type           *type_new(TypeKind k);
Type           *type_ptr(Type *base);
Type           *type_array(Type *base, long sz);
void            type_free(Type *t);

#endif /* VUCA_AST_H */
