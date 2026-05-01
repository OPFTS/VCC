#ifndef VUMC_H
#define VUMC_H

#include "../include/vcc/targets.h"

typedef struct {
    const char **inputs;
    int          input_count;
    const char  *output;
    VCCTarget    target;
    const char **lib_dirs;
    int          lib_dir_count;
    const char **libs;
    int          lib_count;
    int          pic;
    int          pie;
    int          debug;
    int          verbose;
    int          ebpf;
    int          shared;    /* build .so */
    int          static_link;
} VUMCOptions;

int vumc_process(const VUMCOptions *opts);

#endif /* VUMC_H */
