#include "languages.h"
#include <string.h>

VCCLanguage vcc_detect_language(const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (!ext) return LANG_UNKNOWN;
    ext++;
    if (!strcmp(ext,"c"))                            return LANG_C;
    if (!strcmp(ext,"cpp")||!strcmp(ext,"cxx")||
        !strcmp(ext,"cc")||!strcmp(ext,"C"))         return LANG_CPP;
    if (!strcmp(ext,"pas")||!strcmp(ext,"pp"))       return LANG_PASCAL;
    if (!strcmp(ext,"go"))                           return LANG_GO;
    if (!strcmp(ext,"rs"))                           return LANG_RUST;
    if (!strcmp(ext,"s")||!strcmp(ext,"asm"))        return LANG_ASM;
    if (!strcmp(ext,"S"))                            return LANG_ASM_PP;
    if (!strcmp(ext,"ll"))                           return LANG_LLVMIR;
    return LANG_UNKNOWN;
}

const char *vcc_language_name(VCCLanguage lang) {
    switch (lang) {
        case LANG_C:          return "C";
        case LANG_CPP:        return "C++";
        case LANG_PASCAL:     return "Pascal/FreePascal";
        case LANG_FREEPASCAL: return "FreePascal";
        case LANG_GO:         return "Go";
        case LANG_RUST:       return "Rust";
        case LANG_ASM:        return "Assembly";
        case LANG_ASM_PP:     return "Assembly+CPP";
        case LANG_LLVMIR:     return "LLVM IR";
        default:              return "Unknown";
    }
}
