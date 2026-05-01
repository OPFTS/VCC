#ifndef VCC_TARGETS_H
#define VCC_TARGETS_H

typedef enum {
    TARGET_X86_64 = 0,
    TARGET_X86,
    TARGET_AARCH64,
    TARGET_ARM,
    TARGET_RISCV32,
    TARGET_RISCV64,
    TARGET_MIPS,
    TARGET_MIPSEL,
    TARGET_MIPS64,
    TARGET_MIPS64EL,
    TARGET_PPC,
    TARGET_PPC64,
    TARGET_PPC64LE,
    TARGET_S390X,
    TARGET_HPPA,
    TARGET_OR1K,
    TARGET_WASM32,
    TARGET_WASM64,
    TARGET_BPF,
    TARGET_UNKNOWN
} VCCTarget;

typedef struct {
    VCCTarget   id;
    const char *name;       /* short name used in -target */
    const char *triple;     /* LLVM target triple         */
    const char *qemu_cmd;   /* qemu binary name           */
    const char *qemu_flags; /* extra qemu flags           */
    const char *march;      /* -march value               */
} VCCTargetInfo;

VCCTarget            vcc_parse_target(const char *name);
const VCCTargetInfo *vcc_get_target_info(VCCTarget t);
const char          *vcc_target_name(VCCTarget t);

#endif /* VCC_TARGETS_H */
