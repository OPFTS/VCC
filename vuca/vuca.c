#include "vuca.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declarations for each language frontend */
int vuca_c_compile      (const VUCAOptions *opts);
int vuca_cpp_compile    (const VUCAOptions *opts);
int vuca_pascal_compile (const VUCAOptions *opts);
int vuca_go_compile     (const VUCAOptions *opts);
int vuca_rust_compile   (const VUCAOptions *opts);
int vuca_asm_compile    (const VUCAOptions *opts);
int vuca_llvmir_passthru(const VUCAOptions *opts);

int vuca_process(const VUCAOptions *opts) {
    if (opts->verbose)
        fprintf(stderr, "[VUC/A] %s -> %s  (lang=%s, target=%s)\n",
                opts->input, opts->output,
                vcc_language_name(opts->lang),
                vcc_target_name(opts->target));

    switch (opts->lang) {
        case LANG_C:
        case LANG_ASM_PP:    return vuca_c_compile(opts);
        case LANG_CPP:       return vuca_cpp_compile(opts);
        case LANG_PASCAL:
        case LANG_FREEPASCAL:return vuca_pascal_compile(opts);
        case LANG_GO:        return vuca_go_compile(opts);
        case LANG_RUST:      return vuca_rust_compile(opts);
        case LANG_ASM:       return vuca_asm_compile(opts);
        case LANG_LLVMIR:    return vuca_llvmir_passthru(opts);
        default:
            fprintf(stderr, "[VUC/A] error: unsupported language\n");
            return 1;
    }
}
