#include "../vuca.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int vuca_go_compile(const VUCAOptions *opts) {
    char cmd[4096];

    /* Go → LLVM IR via tinygo (if available) or standard go + gollvm */
    /* Try tinygo first (better LLVM support), fall back to standard go */
    snprintf(cmd, sizeof(cmd),
        "which tinygo >/dev/null 2>&1 && "
        "tinygo build -target=%s -o '%s' '%s' || "
        "GOARCH=%s go build -o '%s' '%s'",
        vcc_target_name(opts->target), opts->output, opts->input,
        vcc_target_name(opts->target), opts->output, opts->input);

    if (opts->verbose) fprintf(stderr,"[VUC/A Go] %s\n",cmd);

    /* Standard path: go tool compile to .a, then llvm-dis */
    snprintf(cmd, sizeof(cmd),
        "go tool compile -pack -trimpath -o '%s' '%s'",
        opts->output, opts->input);
    if (opts->verbose) fprintf(stderr,"[VUC/A Go] %s\n",cmd);
    return system(cmd);
}
