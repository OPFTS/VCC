#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../vuca.h"
#include "../../vcp/vcp.h"
#include "lexer.h"
#include "parser.h"
#include "codegen.h"

int vuca_c_compile(const VUCAOptions *opts) {
    char cmd[4096];
    const VCCTargetInfo *ti = vcc_get_target_info(opts->target);

    if (opts->preprocess_only) {
        /* Use VCP for preprocessing */
        VCPCtx *ctx = vcp_create();
        for (int i=0; i<opts->define_count; i++)
            vcp_define(ctx, opts->defines[i], "1");
        for (int i=0; i<opts->include_count; i++)
            vcp_add_include_dir(ctx, opts->include_dirs[i]);
        int r = vcp_process_file(ctx, opts->input, opts->output);
        vcp_free(ctx);
        return r;
    }

    /* VCP preprocess to temp file, then clang for LLVM IR */
    char pp_tmp[256];
    snprintf(pp_tmp, sizeof(pp_tmp), "/tmp/vuca_pp_%d.c", (int)getpid());

    VCPCtx *ctx = vcp_create();
    for (int i=0; i<opts->define_count; i++)
        vcp_define(ctx, opts->defines[i], "1");
    for (int i=0; i<opts->include_count; i++)
        vcp_add_include_dir(ctx, opts->include_dirs[i]);
    int r = vcp_process_file(ctx, opts->input, pp_tmp);
    vcp_free(ctx);

    if (r != 0) {
        fprintf(stderr, "[VUC/A] VCP preprocessing failed: %s\n", opts->input);
        unlink(pp_tmp);
        return 1;
    }

    /* clang: LLVM IR from preprocessed source */
    int n = snprintf(cmd, sizeof(cmd),
        "clang -S -emit-llvm -O%d %s %s"
        " -target %s -std=%s"
        " -x c"               /* already preprocessed */
        " -o '%s' '%s'",
        opts->optimize,
        opts->debug  ? "-g"        : "",
        opts->ebpf   ? "-D__BPF__" : "",
        ti           ? ti->triple  : "x86_64-linux-gnu",
        opts->std    ? opts->std   : "c11",
        opts->output, pp_tmp);

    if (opts->verbose) fprintf(stderr, "[VUC/A C] %s\n", cmd);
    r = system(cmd);
    unlink(pp_tmp);

    if (r != 0) {
        fprintf(stderr, "[VUC/A] compilation failed: %s\n", opts->input);
        return 1;
    }
    return 0;
}
