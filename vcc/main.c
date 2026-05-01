#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "../include/vcc/version.h"
#include "../include/vcc/targets.h"
#include "../include/vcc/languages.h"
#include "../vuca/vuca.h"
#include "../vuamc/vuamc.h"
#include "../vumc/vumc.h"

typedef enum {
    MODE_COMPILE_LINK = 0,
    MODE_COMPILE_ONLY,
    MODE_ASSEMBLE_ONLY,
    MODE_IR_ONLY,
    MODE_PREPROCESS_ONLY,
} CompileMode;

typedef struct {
    char       *input_files[256];
    int         input_count;
    char       *output_file;
    VCCTarget   target;
    CompileMode mode;
    int         optimize;
    int         debug;
    int         verbose;
    char       *include_dirs[64]; int include_count;
    char       *lib_dirs[64];     int lib_dir_count;
    char       *libs[64];         int lib_count;
    char       *defines[64];      int define_count;
    int         pic, pie, ebpf, shared, static_link;
} Opts;

static void usage(const char *prog) {
    printf(
      "vcc — VeroCC Compiler Driver v%s\n\n"
      "Usage: vcc [options] <files...>\n\n"
      "Compile modes:\n"
      "  (default)      compile + link\n"
      "  -c             compile to .o only\n"
      "  -S             emit assembly (.s)\n"
      "  --emit-ir      emit LLVM IR (.ll)\n"
      "  -E             preprocess only\n\n"
      "Options:\n"
      "  -o <file>      output file\n"
      "  -O0..3         optimization level\n"
      "  -g             debug info\n"
      "  -v             verbose\n"
      "  -I<dir>        include path\n"
      "  -L<dir>        library path\n"
      "  -l<lib>        link library\n"
      "  -D<macro>      define macro\n"
      "  -target <t>    target (x86_64,aarch64,riscv64,mips,s390x,...)\n"
      "  --ebpf         eBPF target\n"
      "  -fPIC          position-independent code\n"
      "  -fPIE          position-independent executable\n"
      "  -shared        build shared library\n"
      "  -static        static link\n"
      "  --version      version info\n\n"
      "Supported languages: C  C++  Pascal  Go  Rust  Assembly\n"
      "Supported targets:   x86_64 x86 aarch64 arm riscv32 riscv64\n"
      "                     mips mipsel mips64 ppc ppc64 s390x hppa or1k\n"
      "                     wasm32 wasm64 bpf\n\n"
      "Pipeline:  Source -[VUC/A]-> LLVM IR -[VUA/MC]-> .o -[VUMC]-> binary\n",
      VCC_VERSION_STRING);
}

static char *replace_ext(const char *f, const char *ext) {
    const char *dot   = strrchr(f, '.');
    const char *slash = strrchr(f, '/');
    size_t base = (dot && (!slash||dot>slash)) ? (size_t)(dot-f) : strlen(f);
    char *r = malloc(base + strlen(ext) + 2);
    strncpy(r, f, base); r[base]='.'; strcpy(r+base+1, ext);
    return r;
}

int main(int argc, char **argv) {
    Opts o = {0};
    o.target   = TARGET_X86_64;
    o.optimize = 0;

    if (argc < 2) { usage(argv[0]); return 1; }

    for (int i=1; i<argc; i++) {
        if (!strcmp(argv[i],"--version")) {
            printf("%s v%s\n  %s\n  %s\n  %s\n",
                   VCC_NAME,VCC_VERSION_STRING,VUCA_NAME,VUAMC_NAME,VUMC_NAME);
            return 0;
        }
        else if (!strcmp(argv[i],"-h")||!strcmp(argv[i],"--help")) { usage(argv[0]); return 0; }
        else if (!strcmp(argv[i],"-v"))          o.verbose=1;
        else if (!strcmp(argv[i],"-c"))          o.mode=MODE_COMPILE_ONLY;
        else if (!strcmp(argv[i],"-S"))          o.mode=MODE_ASSEMBLE_ONLY;
        else if (!strcmp(argv[i],"--emit-ir"))   o.mode=MODE_IR_ONLY;
        else if (!strcmp(argv[i],"-E"))          o.mode=MODE_PREPROCESS_ONLY;
        else if (!strcmp(argv[i],"-g"))          o.debug=1;
        else if (!strcmp(argv[i],"--ebpf"))    { o.ebpf=1; o.target=TARGET_BPF; }
        else if (!strcmp(argv[i],"-fPIC"))       o.pic=1;
        else if (!strcmp(argv[i],"-fPIE"))       o.pie=1;
        else if (!strcmp(argv[i],"-shared"))     o.shared=1;
        else if (!strcmp(argv[i],"-static"))     o.static_link=1;
        else if (!strncmp(argv[i],"-O",2))       o.optimize=atoi(argv[i]+2);
        else if (!strcmp(argv[i],"-o")&&i+1<argc)o.output_file=argv[++i];
        else if (!strncmp(argv[i],"-I",2)) {
            o.include_dirs[o.include_count++] =
                strlen(argv[i])>2 ? argv[i]+2 : argv[++i];
        }
        else if (!strncmp(argv[i],"-L",2)) {
            o.lib_dirs[o.lib_dir_count++] =
                strlen(argv[i])>2 ? argv[i]+2 : argv[++i];
        }
        else if (!strncmp(argv[i],"-l",2)) {
            o.libs[o.lib_count++] =
                strlen(argv[i])>2 ? argv[i]+2 : argv[++i];
        }
        else if (!strncmp(argv[i],"-D",2))
            o.defines[o.define_count++] = argv[i]+2;
        else if (!strcmp(argv[i],"-target")&&i+1<argc) {
            o.target = vcc_parse_target(argv[++i]);
            if (o.target==TARGET_UNKNOWN) {
                fprintf(stderr,"vcc: unknown target '%s'\n",argv[i]);
                return 1;
            }
        }
        else if (argv[i][0]!='-')
            o.input_files[o.input_count++] = argv[i];
        else
            fprintf(stderr,"vcc: warning: ignored '%s'\n",argv[i]);
    }

    if (!o.input_count) { fprintf(stderr,"vcc: no input files\n"); return 1; }

    char *obj_files[256]; int obj_count=0;

    for (int i=0; i<o.input_count; i++) {
        const char  *inp  = o.input_files[i];
        VCCLanguage  lang = vcc_detect_language(inp);
        if (lang==LANG_UNKNOWN) {
            fprintf(stderr,"vcc: unknown file type '%s'\n",inp); return 1;
        }
        if (o.verbose)
            fprintf(stderr,"[VCC] %s (%s)\n", inp, vcc_language_name(lang));

        char *ir_file  = replace_ext(inp,"ll");
        char *obj_file = replace_ext(inp,"o");
        char *asm_file = replace_ext(inp,"s");

        /* ── Step 1: VUC/A ─────────────────── */
        if (o.mode==MODE_PREPROCESS_ONLY) {
            VUCAOptions va={.input=inp,.output=o.output_file,.target=o.target,
              .lang=lang,.preprocess_only=1,.verbose=o.verbose,
              .defines=(const char**)o.defines,.define_count=o.define_count,
              .include_dirs=(const char**)o.include_dirs,.include_count=o.include_count};
            int r=vuca_process(&va);
            free(ir_file);free(obj_file);free(asm_file);
            if (r) return r;
            continue;
        }

        VUCAOptions va={
            .input=inp,.output=ir_file,.target=o.target,.lang=lang,
            .emit_ir=1,.optimize=o.optimize,.debug=o.debug,.verbose=o.verbose,
            .ebpf=o.ebpf,
            .defines=(const char**)o.defines,.define_count=o.define_count,
            .include_dirs=(const char**)o.include_dirs,.include_count=o.include_count
        };
        int r=vuca_process(&va);
        if (r) { fprintf(stderr,"vcc: VUC/A failed on '%s'\n",inp);
                 free(ir_file);free(obj_file);free(asm_file); return r; }

        if (o.mode==MODE_IR_ONLY) {
            if (o.output_file) rename(ir_file,o.output_file);
            free(ir_file);free(obj_file);free(asm_file); continue;
        }

        /* ── Step 2: VUA/MC ────────────────── */
        const char *vuamc_out = (o.mode==MODE_ASSEMBLE_ONLY && o.output_file)
                                ? o.output_file : asm_file;
        if (o.mode==MODE_ASSEMBLE_ONLY) {
            VUAMCOptions ma={.input=ir_file,.output=vuamc_out,.target=o.target,
              .emit_asm=1,.optimize=o.optimize,.verbose=o.verbose,.pic=o.pic};
            r=vuamc_process(&ma);
            unlink(ir_file);
            free(ir_file);free(obj_file);free(asm_file);
            if (r) return r; continue;
        }

        const char *obj_out=(o.mode==MODE_COMPILE_ONLY&&o.output_file)
                            ? o.output_file : obj_file;
        VUAMCOptions ma={.input=ir_file,.output=obj_out,.target=o.target,
          .emit_asm=0,.optimize=o.optimize,.debug=o.debug,.verbose=o.verbose,.pic=o.pic};
        r=vuamc_process(&ma);
        unlink(ir_file); free(ir_file); free(asm_file);
        if (r) { free(obj_file); return r; }

        if (o.mode==MODE_COMPILE_ONLY) { free(obj_file); continue; }

        obj_files[obj_count++] = strdup(obj_out);
        free(obj_file);
    }

    /* ── Step 3: VUMC ──────────────────────── */
    if (o.mode==MODE_COMPILE_LINK && obj_count>0) {
        const char *out = o.output_file ? o.output_file : "a.out";
        VUMCOptions vm={
            .inputs=(const char**)obj_files,.input_count=obj_count,
            .output=out,.target=o.target,
            .lib_dirs=(const char**)o.lib_dirs,.lib_dir_count=o.lib_dir_count,
            .libs=(const char**)o.libs,.lib_count=o.lib_count,
            .pic=o.pic,.pie=o.pie,.debug=o.debug,.verbose=o.verbose,
            .ebpf=o.ebpf,.shared=o.shared,.static_link=o.static_link
        };
        int r=vumc_process(&vm);
        for (int i=0;i<obj_count;i++){unlink(obj_files[i]);free(obj_files[i]);}
        if (r) return r;
    }
    if (o.verbose) printf("[VCC] Done.\n");
    return 0;
}
