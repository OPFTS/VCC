#ifndef VCC_LANGUAGES_H
#define VCC_LANGUAGES_H

typedef enum {
    LANG_C = 0,
    LANG_CPP,
    LANG_PASCAL,
    LANG_FREEPASCAL,
    LANG_GO,
    LANG_RUST,
    LANG_ASM,          /* generic assembler  */
    LANG_ASM_PP,       /* .S (with CPP)      */
    LANG_LLVMIR,       /* .ll pre-compiled   */
    LANG_UNKNOWN
} VCCLanguage;

VCCLanguage  vcc_detect_language(const char *filename);
const char  *vcc_language_name(VCCLanguage lang);

#endif /* VCC_LANGUAGES_H */
