cmake_minimum_required(VERSION 3.16)
project(VeroCC VERSION 0.1.5 LANGUAGES C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "LLVM version: ${LLVM_VERSION}")
include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})
add_definitions(${LLVM_DEFINITIONS})

llvm_map_components_to_libnames(LLVM_LIBS
    Core Support Analysis BitWriter Target
    X86 X86AsmParser X86CodeGen X86Desc X86Info
    AArch64 AArch64AsmParser AArch64CodeGen AArch64Desc AArch64Info
    RISCV RISCVAsmParser RISCVCodeGen RISCVDesc RISCVInfo
    Mips MipsAsmParser MipsCodeGen MipsDesc MipsInfo
    PowerPC PowerPCAsmParser PowerPCCodeGen PowerPCDesc PowerPCInfo
    SystemZ SystemZAsmParser SystemZCodeGen SystemZDesc SystemZInfo
    WebAssembly WebAssemblyAsmParser WebAssemblyCodeGen WebAssemblyDesc WebAssemblyInfo
    BPF BPFAsmParser BPFCodeGen BPFDesc BPFInfo
    ARM ARMAsmParser ARMCodeGen ARMDesc ARMInfo
    MC MCParser MCDisassembler
    Passes Linker LTO IRReader IPO
    AllTargetsAsmParsers AllTargetsCodeGens AllTargetsDescs AllTargetsInfos
)

# ── Shared helpers ────────────────────────────────────────────────
add_library(vcc_helpers STATIC
    include/vcc/targets.c
    include/vcc/languages.c
)
target_include_directories(vcc_helpers PUBLIC include)

# ── VCP: Vero C Preprocessor ──────────────────────────────────────
add_library(vcp STATIC vcp/vcp.c)
target_include_directories(vcp PUBLIC vcp)

# ── VUC/A ─────────────────────────────────────────────────────────
add_library(vuca STATIC
    vuca/vuca.c
    vuca/c_frontend/lexer.c
    vuca/c_frontend/ast.c
    vuca/c_frontend/parser.c
    vuca/c_frontend/codegen.c
    vuca/c_frontend/c_frontend.c
    vuca/cpp_frontend/cpp_frontend.c
    vuca/pascal_frontend/pascal_frontend.c
    vuca/go_frontend/go_frontend.c
    vuca/rust_frontend/rust_frontend.c
    vuca/asm_frontend/asm_frontend.c
)
target_include_directories(vuca PUBLIC include vuca vcp)
target_link_libraries(vuca vcc_helpers vcp ${LLVM_LIBS})

# ── VUA/MC ────────────────────────────────────────────────────────
add_library(vuamc STATIC vuamc/vuamc.c)
target_include_directories(vuamc PUBLIC include)
target_link_libraries(vuamc vcc_helpers)

# ── VUMC ──────────────────────────────────────────────────────────
add_library(vumc_lib STATIC vumc/vumc.c)
target_include_directories(vumc_lib PUBLIC include)
target_link_libraries(vumc_lib vcc_helpers)

# ── verolibc ──────────────────────────────────────────────────────
add_library(verolibc STATIC
    lib/verolibc/verolibc.c
    lib/verolibc/vero_malloc.c
    lib/verolibc/vero_string.c
    lib/verolibc/vero_stdio.c
    lib/verolibc/vero_stdlib.c
    lib/verolibc/vero_math.c
)
target_include_directories(verolibc PUBLIC lib/verolibc)
target_compile_options(verolibc PRIVATE -O2 -fno-builtin -ffreestanding)

# ── vcc executable ────────────────────────────────────────────────
add_executable(vcc vcc/main.c)
target_link_libraries(vcc vuca vuamc vumc_lib vcc_helpers vcp ${LLVM_LIBS} m dl z)

# ── vcp standalone tool ───────────────────────────────────────────
add_executable(vcp_tool vcp/vcp_main.c)
set_target_properties(vcp_tool PROPERTIES OUTPUT_NAME "vcp")
target_link_libraries(vcp_tool vcp)

# ── Install ───────────────────────────────────────────────────────
install(TARGETS vcc vcp_tool DESTINATION bin)
install(TARGETS verolibc DESTINATION lib)
install(FILES lib/verolibc/verolibc.h DESTINATION include/vcc)
install(FILES ebpf/vcc_ebpf.h         DESTINATION include/vcc)
install(FILES vcp/vcp.h               DESTINATION include/vcc)
