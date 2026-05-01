#include "../vuca.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int vuca_rust_compile(const VUCAOptions *opts) {
    char cmd[4096];
    const VCCTargetInfo *ti = vcc_get_target_info(opts->target);

    /* rustc → LLVM IR → our pipeline */
    snprintf(cmd, sizeof(cmd),
        "rustc --emit=llvm-ir -C opt-level=%d "
        "--target=%s -o '%s' '%s'",
        opts->optimize,
        ti ? ti->triple : "x86_64-unknown-linux-gnu",
        opts->output,
        opts->input);

    if (opts->verbose) fprintf(stderr,"[VUC/A Rust] %s\n",cmd);
    return system(cmd);
}
