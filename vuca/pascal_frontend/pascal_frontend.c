#include "../vuca.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int vuca_pascal_compile(const VUCAOptions *opts) {
    /* FreePascal → obj → llvm-objcopy → we reassemble with lld */
    char cmd[4096];
    char obj_tmp[256];
    char asm_tmp[256];
    snprintf(obj_tmp, sizeof(obj_tmp), "/tmp/vuca_fpc_%d.o", (int)getpid());
    snprintf(asm_tmp, sizeof(asm_tmp), "/tmp/vuca_fpc_%d.s", (int)getpid());

    /* Compile with FPC to native object */
    snprintf(cmd, sizeof(cmd),
        "fpc -O%d -o%s '%s' -FE/tmp 2>&1",
        opts->optimize, obj_tmp, opts->input);
    if (opts->verbose) fprintf(stderr,"[VUC/A Pascal] %s\n",cmd);
    int ret = system(cmd);
    if (ret != 0) {
        fprintf(stderr,"[VUC/A Pascal] fpc failed\n");
        return 1;
    }

    /* Disassemble to LLVM IR-like text assembly */
    snprintf(cmd, sizeof(cmd),
        "llvm-objdump --disassemble-all '%s' > '%s'", obj_tmp, asm_tmp);
    system(cmd);

    /* For full LLVM IR: use clang as bridge via C wrapper (advanced: TODO) */
    /* For now, copy obj as-is and skip VUCA/VUAMC, go straight to VUMC */
    snprintf(cmd, sizeof(cmd), "cp '%s' '%s'", obj_tmp, opts->output);
    ret = system(cmd);
    unlink(obj_tmp); unlink(asm_tmp);
    return ret;
}
