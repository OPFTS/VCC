#include "vcp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    VCPCtx *ctx = vcp_create();
    const char *input  = NULL;
    const char *output = NULL;

    for (int i = 1; i < argc; i++) {
        if (!strncmp(argv[i], "-D", 2)) {
            char *eq = strchr(argv[i]+2, '=');
            if (eq) { *eq='\0'; vcp_define(ctx, argv[i]+2, eq+1); *eq='='; }
            else      vcp_define(ctx, argv[i]+2, "1");
        } else if (!strncmp(argv[i], "-I", 2)) {
            vcp_add_include_dir(ctx, argv[i]+2);
        } else if (!strcmp(argv[i], "-o") && i+1 < argc) {
            output = argv[++i];
        } else if (argv[i][0] != '-') {
            input = argv[i];
        }
    }

    if (!input) { fprintf(stderr, "vcp: no input file\n"); return 1; }
    int ret = vcp_process_file(ctx, input, output);
    vcp_free(ctx);
    return ret;
}
