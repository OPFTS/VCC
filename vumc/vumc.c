#include "vumc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Detect CRT paths at runtime */
static const char *find_crt(const char *file) {
    static char path[512];
    /* Fedora/RHEL paths */
    const char *dirs[] = {
        "/usr/lib64",
        "/usr/lib",
        "/usr/lib/x86_64-linux-gnu",   /* Debian/Ubuntu */
        "/usr/lib64/linux-gnu",
        NULL
    };
    for (int i = 0; dirs[i]; i++) {
        snprintf(path, sizeof(path), "%s/%s", dirs[i], file);
        FILE *f = fopen(path, "r");
        if (f) { fclose(f); return path; }
    }
    return NULL;
}

/* Find libc for the linker */
static const char *find_libdir(void) {
    const char *dirs[] = {
        "/usr/lib64",
        "/usr/lib/x86_64-linux-gnu",
        "/usr/lib",
        NULL
    };
    for (int i = 0; dirs[i]; i++) {
        char path[512];
        snprintf(path, sizeof(path), "%s/libc.so", dirs[i]);
        FILE *f = fopen(path, "r");
        if (f) { fclose(f); return dirs[i]; }
        snprintf(path, sizeof(path), "%s/libc.so.6", dirs[i]);
        f = fopen(path, "r");
        if (f) { fclose(f); return dirs[i]; }
    }
    return "/usr/lib64";
}

int vumc_process(const VUMCOptions *opts) {
    char cmd[8192];
    const VCCTargetInfo *ti = vcc_get_target_info(opts->target);

    if (opts->verbose)
        fprintf(stderr, "[VUMC] linking %d object(s) -> %s\n",
                opts->input_count, opts->output);

    /* eBPF: no system linker needed */
    if (opts->ebpf) {
        if (opts->input_count == 1) {
            snprintf(cmd, sizeof(cmd),
                "llvm-strip -g '%s' 2>/dev/null; cp '%s' '%s'",
                opts->inputs[0], opts->inputs[0], opts->output);
        } else {
            int n = snprintf(cmd, sizeof(cmd), "ld.bfd -r -o '%s'", opts->output);
            for (int i = 0; i < opts->input_count; i++)
                n += snprintf(cmd+n, sizeof(cmd)-n, " '%s'", opts->inputs[i]);
        }
        return system(cmd);
    }

    /* Use clang as linker driver — it knows all the right paths automatically */
    int n = snprintf(cmd, sizeof(cmd), "clang");

    if (opts->shared)      n += snprintf(cmd+n, sizeof(cmd)-n, " -shared");
    if (opts->static_link) n += snprintf(cmd+n, sizeof(cmd)-n, " -static");
    if (opts->pie && !opts->shared)
                           n += snprintf(cmd+n, sizeof(cmd)-n, " -pie");
    if (opts->debug)       n += snprintf(cmd+n, sizeof(cmd)-n, " -g");

    /* target triple */
    if (ti && ti->triple)
        n += snprintf(cmd+n, sizeof(cmd)-n, " -target %s", ti->triple);

    n += snprintf(cmd+n, sizeof(cmd)-n, " -o '%s'", opts->output);

    /* input objects */
    for (int i = 0; i < opts->input_count; i++)
        n += snprintf(cmd+n, sizeof(cmd)-n, " '%s'", opts->inputs[i]);

    /* library directories */
    for (int i = 0; i < opts->lib_dir_count; i++)
        n += snprintf(cmd+n, sizeof(cmd)-n, " -L'%s'", opts->lib_dirs[i]);

    /* libraries */
    for (int i = 0; i < opts->lib_count; i++)
        n += snprintf(cmd+n, sizeof(cmd)-n, " -l%s", opts->libs[i]);

    /* use lld as the backend linker */
    n += snprintf(cmd+n, sizeof(cmd)-n, " -fuse-ld=lld");

    if (opts->verbose) fprintf(stderr, "[VUMC] cmd: %s\n", cmd);
    int ret = system(cmd);
    if (ret != 0) {
        fprintf(stderr, "[VUMC] linking failed (exit %d)\n", ret);
        return 1;
    }
    return 0;
}
