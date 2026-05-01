#include "../vuca.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int vuca_cpp_compile(const VUCAOptions *opts) {
    char cmd[4096];
    const VCCTargetInfo *ti = vcc_get_target_info(opts->target);

    /* Use clang++ to emit LLVM IR, then we take it through our pipeline */
    int n = snprintf(cmd, sizeof(cmd),
        "clang++ -S -emit-llvm -O%d %s %s -target %s",
        opts->optimize,
        opts->debug ? "-g" : "",
        opts->ebpf  ? "-D__BPF__" : "",
        ti ? ti->triple : "x86_64-linux-gnu");

    for (int i=0; i<opts->define_count; i++)
        n += snprintf(cmd+n, sizeof(cmd)-n, " -D%s", opts->defines[i]);
    for (int i=0; i<opts->include_count; i++)
        n += snprintf(cmd+n, sizeof(cmd)-n, " -I%s", opts->include_dirs[i]);

    snprintf(cmd+n, sizeof(cmd)-n, " -o '%s' '%s'",
             opts->output, opts->input);

    if (opts->verbose) fprintf(stderr, "[VUC/A C++] %s\n", cmd);
    return system(cmd);
}
