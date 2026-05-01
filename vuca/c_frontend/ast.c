#include "ast.h"
#include <stdlib.h>
#include <string.h>

ASTNode *ast_node_new(void) {
    return calloc(1, sizeof(ASTNode));
}

void ast_node_free(ASTNode *n) {
    if (!n) return;
    /* TODO: deep free – fine for now; we don't free during codegen */
    free(n);
}

TranslationUnit *tu_new(void) {
    return calloc(1, sizeof(TranslationUnit));
}

void tu_free(TranslationUnit *tu) {
    if (!tu) return;
    for (int i = 0; i < tu->count; i++) ast_node_free(tu->decls[i]);
    free(tu->decls);
    free(tu);
}

Type *type_new(TypeKind k) {
    Type *t = calloc(1, sizeof(Type));
    t->kind = k;
    t->array_size = -1;
    return t;
}

Type *type_ptr(Type *base) {
    Type *t = type_new(TY_PTR);
    t->base = base;
    return t;
}

Type *type_array(Type *base, long sz) {
    Type *t = type_new(TY_ARRAY);
    t->base = base;
    t->array_size = sz;
    return t;
}

void type_free(Type *t) {
    if (!t) return;
    free(t->name);
    type_free(t->base);
    free(t);
}
