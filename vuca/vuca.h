#ifndef VUCA_H
#define VUCA_H

#include "../include/vcc/targets.h"
#include "../include/vcc/languages.h"

typedef struct {
    const char  *input;
    const char  *output;
    VCCTarget    target;
    VCCLanguage  lang;
    int          emit_ir;          /* output .ll instead of .s  */
    int          preprocess_only;
    int          optimize;         /* 0-3                        */
    int          debug;
    int          verbose;
    int          ebpf;
    int          pic;
    const char **defines;
    int          define_count;
    const char **include_dirs;
    int          include_count;
    const char  *std;              /* "c99","c11","c++17", …     */
} VUCAOptions;

int vuca_process(const VUCAOptions *opts);

#endif /* VUCA_H */
