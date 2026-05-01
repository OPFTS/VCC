#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../vuca.h"
#include "lexer.h"
#include "parser.h"
#include "codegen.h"

/* Use our own parser only when --native-parser flag is set.
   Default: delegate to clang (handles all system headers correctly). */

int vuca_c_compile(const VUCAOptions *opts) {
    char cmd[4096];
    const VCCTargetInfo *ti = vcc_get_target_info(opts->target);

    if (opts->preprocess_only) {
        int n = snprintf(cmd, sizeof(cmd), "cpp -w");
        for (int i = 0; i < opts->define_count; i++)
            n += snprintf(cmd+n, sizeof(cmd)-n, " -D%s", opts->defines[i]);
        for (int i = 0; i < opts->include_count; i++)
            n += snprintf(cmd+n, sizeof(cmd)-n, " -I%s", opts->include_dirs[i]);
        const char *out = opts->output ? opts->output : "-";
        snprintf(cmd+n, sizeof(cmd)-n, " '%s' -o '%s'", opts->input, out);
        if (opts->verbose) fprintf(stderr, "[VUC/A C] %s\n", cmd);
        return system(cmd);
    }

    /* Emit LLVM IR via clang */
    int n = snprintf(cmd, sizeof(cmd),
        "clang -S -emit-llvm -O%d %s %s"
        " -target %s"
        " -std=%s",
        opts->optimize,
        opts->debug  ? "-g"        : "",
        opts->ebpf   ? "-D__BPF__" : "",
        ti           ? ti->triple  : "x86_64-linux-gnu",
        opts->std    ? opts->std   : "c11");

    for (int i = 0; i < opts->define_count; i++)
        n += snprintf(cmd+n, sizeof(cmd)-n, " -D%s", opts->defines[i]);
    for (int i = 0; i < opts->include_count; i++)
        n += snprintf(cmd+n, sizeof(cmd)-n, " -I%s", opts->include_dirs[i]);

    snprintf(cmd+n, sizeof(cmd)-n, " -o '%s' '%s'", opts->output, opts->input);

    if (opts->verbose) fprintf(stderr, "[VUC/A C] %s\n", cmd);
    int ret = system(cmd);
    if (ret != 0) {
        fprintf(stderr, "[VUC/A] clang frontend failed on '%s'\n", opts->input);
        return 1;
    }
    return 0;
}
