#include "targets.h"
#include <string.h>
#include <stdio.h>

static const VCCTargetInfo TARGETS[] = {
    { TARGET_X86_64,  "x86_64",   "x86_64-linux-gnu",
      "qemu-x86_64",  "",                          "x86-64"      },
    { TARGET_X86,     "x86",      "i686-linux-gnu",
      "qemu-i386",    "",                          "i686"        },
    { TARGET_AARCH64, "aarch64",  "aarch64-linux-gnu",
      "qemu-aarch64", "-L /usr/aarch64-linux-gnu", "armv8-a"    },
    { TARGET_ARM,     "arm",      "arm-linux-gnueabihf",
      "qemu-arm",     "-L /usr/arm-linux-gnueabihf","armv7-a"   },
    { TARGET_RISCV32, "riscv32",  "riscv32-linux-gnu",
      "qemu-riscv32", "-L /usr/riscv32-linux-gnu", "rv32gc"     },
    { TARGET_RISCV64, "riscv64",  "riscv64-linux-gnu",
      "qemu-riscv64", "-L /usr/riscv64-linux-gnu", "rv64gc"     },
    { TARGET_MIPS,    "mips",     "mips-linux-gnu",
      "qemu-mips",    "-L /usr/mips-linux-gnu",    "mips32r2"   },
    { TARGET_MIPSEL,  "mipsel",   "mipsel-linux-gnu",
      "qemu-mipsel",  "-L /usr/mipsel-linux-gnu",  "mips32r2"   },
    { TARGET_MIPS64,  "mips64",   "mips64-linux-gnuabi64",
      "qemu-mips64",  "-L /usr/mips64-linux-gnuabi64","mips64r2"},
    { TARGET_MIPS64EL,"mips64el", "mips64el-linux-gnuabi64",
      "qemu-mips64el","-L /usr/mips64el-linux-gnuabi64","mips64r2"},
    { TARGET_PPC,     "ppc",      "powerpc-linux-gnu",
      "qemu-ppc",     "-L /usr/powerpc-linux-gnu", "ppc"        },
    { TARGET_PPC64,   "ppc64",    "powerpc64-linux-gnu",
      "qemu-ppc64",   "-L /usr/powerpc64-linux-gnu","ppc64"     },
    { TARGET_PPC64LE, "ppc64le",  "powerpc64le-linux-gnu",
      "qemu-ppc64le", "-L /usr/powerpc64le-linux-gnu","ppc64le" },
    { TARGET_S390X,   "s390x",    "s390x-linux-gnu",
      "qemu-s390x",   "-L /usr/s390x-linux-gnu",   "z14"        },
    { TARGET_HPPA,    "hppa",     "hppa-linux-gnu",
      "qemu-hppa",    "",                          "1.1"         },
    { TARGET_OR1K,    "or1k",     "or1k-linux-gnu",
      "qemu-or1k",    "",                          "or1200"      },
    { TARGET_WASM32,  "wasm32",   "wasm32-unknown-unknown",
      NULL,           "",                          ""            },
    { TARGET_WASM64,  "wasm64",   "wasm64-unknown-unknown",
      NULL,           "",                          ""            },
    { TARGET_BPF,     "bpf",      "bpf-unknown-unknown",
      NULL,           "",                          ""            },
    { TARGET_UNKNOWN, NULL, NULL, NULL, NULL, NULL }
};

VCCTarget vcc_parse_target(const char *name) {
    for (int i = 0; TARGETS[i].name != NULL; i++) {
        if (strcmp(TARGETS[i].name, name) == 0 ||
            strcmp(TARGETS[i].triple, name) == 0)
            return TARGETS[i].id;
    }
    return TARGET_UNKNOWN;
}

const VCCTargetInfo *vcc_get_target_info(VCCTarget t) {
    for (int i = 0; TARGETS[i].name != NULL; i++) {
        if (TARGETS[i].id == t) return &TARGETS[i];
    }
    return &TARGETS[TARGET_UNKNOWN];
}

const char *vcc_target_name(VCCTarget t) {
    const VCCTargetInfo *info = vcc_get_target_info(t);
    return info ? info->name : "unknown";
}
