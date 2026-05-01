#ifndef VUAMC_H
#define VUAMC_H

#include "../include/vcc/targets.h"

typedef struct {
    const char  *input;
    const char  *output;
    VCCTarget    target;
    int          emit_asm;   /* 1=.s, 0=.o  */
    int          optimize;
    int          debug;
    int          verbose;
    int          pic;
} VUAMCOptions;

int vuamc_process(const VUAMCOptions *opts);

#endif /* VUAMC_H */
