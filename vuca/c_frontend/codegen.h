#ifndef VUCA_CODEGEN_H
#define VUCA_CODEGEN_H

#include "ast.h"
#include "../../include/vcc/targets.h"

typedef struct CodegenCtx CodegenCtx;

CodegenCtx *codegen_create(const char *module_name,
                            VCCTarget target,
                            int optimize,
                            int debug);
int         codegen_emit(CodegenCtx *ctx, TranslationUnit *tu,
                         const char *output_file, int emit_ir);
void        codegen_free(CodegenCtx *ctx);

#endif /* VUCA_CODEGEN_H */
