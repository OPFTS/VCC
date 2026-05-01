#ifndef VUCA_PARSER_H
#define VUCA_PARSER_H

#include "lexer.h"
#include "ast.h"

typedef struct {
    Lexer          *lex;
    const char     *filename;
    int             errors;
} Parser;

Parser         *parser_create(Lexer *lex, const char *filename);
void            parser_free(Parser *p);
TranslationUnit*parser_parse(Parser *p);

#endif /* VUCA_PARSER_H */
