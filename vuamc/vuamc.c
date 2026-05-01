#include "vuamc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int vuamc_process(const VUAMCOptions *opts) {
    char cmd[4096];
    const VCCTargetInfo *ti = vcc_get_target_info(opts->target);
    const char *triple = ti ? ti->triple : "x86_64-linux-gnu";

    if (opts->verbose)
        fprintf(stderr,"[VUA/MC] %s -> %s  (target=%s)\n",
                opts->input, opts->output, triple);

    if (opts->emit_asm) {
        /* .ll → .s  using llc */
        snprintf(cmd, sizeof(cmd),
            "llc -O%d -mtriple=%s %s -filetype=asm -o '%s' '%s'",
            opts->optimize, triple,
            opts->pic ? "-relocation-model=pic" : "",
            opts->output, opts->input);
    } else {
        /* .ll → .o  using llc */
        snprintf(cmd, sizeof(cmd),
            "llc -O%d -mtriple=%s %s -filetype=obj -o '%s' '%s'",
            opts->optimize, triple,
            opts->pic ? "-relocation-model=pic" : "",
            opts->output, opts->input);
    }

    if (opts->verbose) fprintf(stderr,"[VUA/MC] cmd: %s\n", cmd);
    int ret = system(cmd);
    if (ret != 0) {
        fprintf(stderr,"[VUA/MC] llc failed (exit %d)\n", ret);
        return 1;
    }
    return 0;
}
