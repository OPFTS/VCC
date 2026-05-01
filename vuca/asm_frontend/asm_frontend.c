#include "../vuca.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int vuca_asm_compile(const VUCAOptions *opts) {
    char cmd[4096];
    const VCCTargetInfo *ti = vcc_get_target_info(opts->target);

    /* Use llvm-mc to parse and assemble to LLVM IR or object */
    if (opts->emit_ir) {
        /* Convert .s → .ll via llvm-mc */
        snprintf(cmd, sizeof(cmd),
            "llvm-mc -assemble -filetype=null "
            "--triple=%s '%s' && "
            "cp '%s' '%s'",        /* passthrough for now */
            ti ? ti->triple : "x86_64-linux-gnu",
            opts->input,
            opts->input, opts->output);
    } else {
        /* Assemble directly to object */
        snprintf(cmd, sizeof(cmd),
            "llvm-mc -assemble -filetype=obj "
            "--triple=%s -o '%s' '%s'",
            ti ? ti->triple : "x86_64-linux-gnu",
            opts->output, opts->input);
    }

    if (opts->verbose) fprintf(stderr,"[VUC/A ASM] %s\n",cmd);
    return system(cmd);
}

int vuca_llvmir_passthru(const VUCAOptions *opts) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "cp '%s' '%s'", opts->input, opts->output);
    return system(cmd);
}
