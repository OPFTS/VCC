#include "vcp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

/* ── Context ─────────────────────────────────────────────────── */
struct VCPCtx {
    VCPMacro   macros[VCP_MAX_MACROS];
    int        macro_count;
    char      *include_dirs[VCP_MAX_INCLUDES];
    int        include_count;
    int        depth;        /* include nesting depth */
    int        errors;
};

VCPCtx *vcp_create(void) {
    VCPCtx *ctx = calloc(1, sizeof(VCPCtx));
    /* predefined macros */
    vcp_define(ctx, "__VCP__",    "1");
    vcp_define(ctx, "__VEROCC__", "1");
    vcp_define(ctx, "__linux__",  "1");
    vcp_define(ctx, "__unix__",   "1");
    /* standard include paths */
    vcp_add_include_dir(ctx, "/usr/include");
    vcp_add_include_dir(ctx, "/usr/local/include");
    return ctx;
}

void vcp_free(VCPCtx *ctx) {
    if (!ctx) return;
    for (int i = 0; i < ctx->macro_count; i++) {
        free(ctx->macros[i].name);
        free(ctx->macros[i].body);
    }
    for (int i = 0; i < ctx->include_count; i++)
        free(ctx->include_dirs[i]);
    free(ctx);
}

void vcp_add_include_dir(VCPCtx *ctx, const char *dir) {
    if (ctx->include_count < VCP_MAX_INCLUDES)
        ctx->include_dirs[ctx->include_count++] = strdup(dir);
}

/* ── Macro table ──────────────────────────────────────────────── */
static VCPMacro *find_macro(VCPCtx *ctx, const char *name) {
    for (int i = 0; i < ctx->macro_count; i++)
        if (!strcmp(ctx->macros[i].name, name))
            return &ctx->macros[i];
    return NULL;
}

void vcp_define(VCPCtx *ctx, const char *name, const char *val) {
    VCPMacro *m = find_macro(ctx, name);
    if (!m) {
        if (ctx->macro_count >= VCP_MAX_MACROS) return;
        m = &ctx->macros[ctx->macro_count++];
    } else {
        free(m->name); free(m->body);
    }
    m->name = strdup(name);
    m->body = strdup(val ? val : "");
    m->is_func_like = 0;
    m->param_count  = 0;
}

void vcp_undefine(VCPCtx *ctx, const char *name) {
    for (int i = 0; i < ctx->macro_count; i++) {
        if (!strcmp(ctx->macros[i].name, name)) {
            free(ctx->macros[i].name);
            free(ctx->macros[i].body);
            ctx->macros[i] = ctx->macros[--ctx->macro_count];
            return;
        }
    }
}

/* ── String utilities ─────────────────────────────────────────── */
static char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    char *e = s + strlen(s) - 1;
    while (e > s && isspace((unsigned char)*e)) *e-- = '\0';
    return s;
}

static int starts_with(const char *s, const char *p) {
    return strncmp(s, p, strlen(p)) == 0;
}

/* ── Macro expansion (simple, non-recursive) ─────────────────── */
static char *expand_macros(VCPCtx *ctx, const char *line) {
    char *out = malloc(VCP_LINE_MAX * 4);
    char *op  = out;
    const char *p = line;

    while (*p) {
        /* string literal — copy verbatim */
        if (*p == '"') {
            *op++ = *p++;
            while (*p && *p != '"') {
                if (*p == '\\') *op++ = *p++;
                *op++ = *p++;
            }
            if (*p) *op++ = *p++;
            continue;
        }
        /* char literal */
        if (*p == '\'') {
            *op++ = *p++;
            while (*p && *p != '\'') {
                if (*p == '\\') *op++ = *p++;
                *op++ = *p++;
            }
            if (*p) *op++ = *p++;
            continue;
        }
        /* identifier? */
        if (isalpha((unsigned char)*p) || *p == '_') {
            char ident[256]; int ii = 0;
            while (isalnum((unsigned char)*p) || *p == '_')
                ident[ii++] = *p++;
            ident[ii] = '\0';

            VCPMacro *m = find_macro(ctx, ident);
            if (m && !m->is_func_like) {
                /* simple replacement */
                strcpy(op, m->body);
                op += strlen(m->body);
            } else if (m && m->is_func_like && *p == '(') {
                /* function-like: collect args */
                p++; /* skip ( */
                char args[16][512]; int ac = 0;
                int depth = 1; char *ap = args[0]; ac = 1;
                while (*p && depth > 0) {
                    if (*p == '(') { depth++; *ap++ = *p++; }
                    else if (*p == ')') {
                        depth--;
                        if (depth > 0) *ap++ = *p++;
                        else p++;
                    } else if (*p == ',' && depth == 1) {
                        *ap = '\0'; ap = args[ac++]; p++;
                    } else *ap++ = *p++;
                }
                *ap = '\0';
                /* substitute */
                const char *bp = m->body;
                while (*bp) {
                    if (isalpha((unsigned char)*bp)||*bp=='_') {
                        char pn[64]; int pi=0;
                        while(isalnum((unsigned char)*bp)||*bp=='_')
                            pn[pi++]=*bp++;
                        pn[pi]='\0';
                        int found=0;
                        for(int i=0;i<m->param_count;i++) {
                            if(!strcmp(pn,m->params[i])) {
                                strcpy(op,args[i]);
                                op+=strlen(args[i]);
                                found=1; break;
                            }
                        }
                        if(!found){strcpy(op,pn);op+=strlen(pn);}
                    } else *op++=*bp++;
                }
            } else {
                strcpy(op, ident);
                op += strlen(ident);
            }
            continue;
        }
        *op++ = *p++;
    }
    *op = '\0';
    return out;
}

/* ── Expression evaluator for #if ────────────────────────────── */
static long eval_expr(VCPCtx *ctx, const char *expr);

static long eval_primary(VCPCtx *ctx, const char **p) {
    while (isspace((unsigned char)**p)) (*p)++;
    if (**p == '(') {
        (*p)++;
        long v = eval_expr(ctx, *p);
        /* skip to ) */
        while (**p && **p != ')') (*p)++;
        if (**p) (*p)++;
        return v;
    }
    if (strncmp(*p, "defined", 7) == 0 &&
        !isalnum((unsigned char)(*p)[7]) && (*p)[7] != '_') {
        *p += 7;
        while (isspace((unsigned char)**p)) (*p)++;
        int paren = (**p == '(');
        if (paren) (*p)++;
        while (isspace((unsigned char)**p)) (*p)++;
        char name[128]; int ni = 0;
        while (isalnum((unsigned char)**p) || **p == '_')
            name[ni++] = *(*p)++;
        name[ni] = '\0';
        if (paren) { while (**p && **p != ')') (*p)++; if (**p) (*p)++; }
        return find_macro(ctx, name) ? 1 : 0;
    }
    if (**p == '!') {
        (*p)++;
        return !eval_primary(ctx, p);
    }
    if (**p == '-') {
        (*p)++;
        return -eval_primary(ctx, p);
    }
    if (isdigit((unsigned char)**p) || **p == '0') {
        char *end;
        long v = strtol(*p, &end, 0);
        *p = end;
        return v;
    }
    /* identifier — expand macros, re-evaluate */
    if (isalpha((unsigned char)**p) || **p == '_') {
        char name[128]; int ni = 0;
        while (isalnum((unsigned char)**p) || **p == '_')
            name[ni++] = *(*p)++;
        name[ni] = '\0';
        VCPMacro *m = find_macro(ctx, name);
        if (!m) return 0;
        const char *bp = m->body;
        return eval_primary(ctx, &bp);
    }
    return 0;
}

static long eval_expr(VCPCtx *ctx, const char *expr) {
    const char *p = expr;
    long left = eval_primary(ctx, &p);
    while (*p) {
        while (isspace((unsigned char)*p)) p++;
        if      (strncmp(p,"||",2)==0){p+=2;long r=eval_primary(ctx,&p);left=left||r;}
        else if (strncmp(p,"&&",2)==0){p+=2;long r=eval_primary(ctx,&p);left=left&&r;}
        else if (strncmp(p,"==",2)==0){p+=2;long r=eval_primary(ctx,&p);left=(left==r);}
        else if (strncmp(p,"!=",2)==0){p+=2;long r=eval_primary(ctx,&p);left=(left!=r);}
        else if (strncmp(p,">=",2)==0){p+=2;long r=eval_primary(ctx,&p);left=(left>=r);}
        else if (strncmp(p,"<=",2)==0){p+=2;long r=eval_primary(ctx,&p);left=(left<=r);}
        else if (*p=='>'){p++;long r=eval_primary(ctx,&p);left=(left>r);}
        else if (*p=='<'){p++;long r=eval_primary(ctx,&p);left=(left<r);}
        else if (*p=='+'){p++;long r=eval_primary(ctx,&p);left+=r;}
        else if (*p=='-'){p++;long r=eval_primary(ctx,&p);left-=r;}
        else if (*p=='*'){p++;long r=eval_primary(ctx,&p);left*=r;}
        else if (*p=='/'){p++;long r=eval_primary(ctx,&p);if(r)left/=r;}
        else if (*p=='%'){p++;long r=eval_primary(ctx,&p);if(r)left%=r;}
        else if (*p=='&'){p++;long r=eval_primary(ctx,&p);left&=r;}
        else if (*p=='|'){p++;long r=eval_primary(ctx,&p);left|=r;}
        else if (*p=='^'){p++;long r=eval_primary(ctx,&p);left^=r;}
        else break;
    }
    return left;
}

/* ── Find include file ────────────────────────────────────────── */
static FILE *open_include(VCPCtx *ctx, const char *name,
                           int is_system, const char *cur_dir,
                           char *found_path) {
    char path[1024];
    FILE *f;

    if (!is_system && cur_dir) {
        snprintf(path, sizeof(path), "%s/%s", cur_dir, name);
        f = fopen(path, "r");
        if (f) { strcpy(found_path, path); return f; }
    }
    for (int i = 0; i < ctx->include_count; i++) {
        snprintf(path, sizeof(path), "%s/%s",
                 ctx->include_dirs[i], name);
        f = fopen(path, "r");
        if (f) { strcpy(found_path, path); return f; }
    }
    return NULL;
}

/* ── Process a single file (recursive for includes) ──────────── */
static int process_stream(VCPCtx *ctx, FILE *in,
                           const char *filename,
                           FILE *out, int depth) {
    if (depth > VCP_MAX_DEPTH) {
        fprintf(stderr, "[VCP] error: #include depth exceeded\n");
        return 1;
    }

    char line[VCP_LINE_MAX];
    int  lineno   = 0;
    int  skipping = 0;          /* >0 = inside false branch   */
    int  if_depth = 0;          /* nesting depth of #if stack */
    /* skip_stack: 1=currently skipping this level */
    int  skip_stack[VCP_MAX_DEPTH]; memset(skip_stack,0,sizeof(skip_stack));
    int  done_stack[VCP_MAX_DEPTH]; memset(done_stack,0,sizeof(done_stack));

    /* get directory of current file for relative includes */
    char cur_dir[512] = "";
    const char *slash = strrchr(filename, '/');
    if (slash) { strncpy(cur_dir, filename, slash-filename); cur_dir[slash-filename]='\0'; }

    while (fgets(line, sizeof(line), in)) {
        lineno++;
        /* line continuation */
        while (strlen(line) > 1 &&
               line[strlen(line)-2] == '\\' &&
               line[strlen(line)-1] == '\n') {
            line[strlen(line)-2] = '\0';
            if (!fgets(line+strlen(line), sizeof(line)-strlen(line), in)) break;
            lineno++;
        }

        char *s = trim(line);

        if (*s != '#') {
            /* not a directive */
            if (!skipping) {
                char *expanded = expand_macros(ctx, s);
                fprintf(out, "%s\n", expanded);
                free(expanded);
            }
            continue;
        }

        /* directive */
        s++; /* skip # */
        while (isspace((unsigned char)*s)) s++;

        /* ── #if / #ifdef / #ifndef ── */
        if (starts_with(s, "ifdef")) {
            char *name = trim(s + 5);
            if_depth++;
            if (skipping) {
                skip_stack[if_depth] = 1;
                done_stack[if_depth] = 0;
            } else {
                int taken = find_macro(ctx, name) != NULL;
                skip_stack[if_depth] = !taken;
                done_stack[if_depth] = taken;
                skipping = !taken;
            }
        } else if (starts_with(s, "ifndef")) {
            char *name = trim(s + 6);
            if_depth++;
            if (skipping) {
                skip_stack[if_depth] = 1;
                done_stack[if_depth] = 0;
            } else {
                int taken = find_macro(ctx, name) == NULL;
                skip_stack[if_depth] = !taken;
                done_stack[if_depth] = taken;
                skipping = !taken;
            }
        } else if (starts_with(s, "if")) {
            char *expr = trim(s + 2);
            if_depth++;
            if (skipping) {
                skip_stack[if_depth] = 1;
                done_stack[if_depth] = 0;
            } else {
                long v = eval_expr(ctx, expr);
                skip_stack[if_depth] = !v;
                done_stack[if_depth] = (v != 0);
                skipping = !v;
            }
        } else if (starts_with(s, "elif")) {
            char *expr = trim(s + 4);
            if (if_depth > 0) {
                if (done_stack[if_depth]) {
                    skip_stack[if_depth] = 1;
                    skipping = 1;
                } else {
                    long v = eval_expr(ctx, expr);
                    skip_stack[if_depth] = !v;
                    done_stack[if_depth] = (v != 0);
                    skipping = !v;
                }
            }
        } else if (starts_with(s, "else")) {
            if (if_depth > 0) {
                if (done_stack[if_depth]) {
                    skip_stack[if_depth] = 1;
                    skipping = 1;
                } else {
                    skip_stack[if_depth] = 0;
                    skipping = 0;
                }
            }
        } else if (starts_with(s, "endif")) {
            if (if_depth > 0) {
                if_depth--;
                skipping = (if_depth > 0) ? skip_stack[if_depth] : 0;
            }
        } else if (!skipping) {
            /* only process other directives when not skipping */
            if (starts_with(s, "define")) {
                char *rest = trim(s + 6);
                /* macro name */
                char name[128]; int ni = 0;
                while (isalnum((unsigned char)*rest)||*rest=='_')
                    name[ni++] = *rest++;
                name[ni] = '\0';

                VCPMacro *m;
                if (find_macro(ctx, name)) vcp_undefine(ctx, name);
                m = &ctx->macros[ctx->macro_count++];
                m->name = strdup(name);
                m->param_count = 0;
                m->is_func_like = 0;

                if (*rest == '(') {
                    /* function-like macro */
                    rest++;
                    m->is_func_like = 1;
                    while (*rest && *rest != ')') {
                        while (isspace((unsigned char)*rest)) rest++;
                        char pn[64]; int pi=0;
                        while(isalnum((unsigned char)*rest)||*rest=='_')
                            pn[pi++]=*rest++;
                        pn[pi]='\0';
                        if (pi > 0)
                            m->params[m->param_count++] = strdup(pn);
                        while (isspace((unsigned char)*rest)) rest++;
                        if (*rest == ',') rest++;
                    }
                    if (*rest == ')') rest++;
                }
                m->body = strdup(trim(rest));

            } else if (starts_with(s, "undef")) {
                vcp_undefine(ctx, trim(s + 5));

            } else if (starts_with(s, "include")) {
                char *inc = trim(s + 7);
                int is_system = 0;
                char name[512];
                if (*inc == '<') {
                    is_system = 1;
                    inc++;
                    char *end = strchr(inc, '>');
                    if (end) { strncpy(name, inc, end-inc); name[end-inc]='\0'; }
                } else if (*inc == '"') {
                    inc++;
                    char *end = strchr(inc, '"');
                    if (end) { strncpy(name, inc, end-inc); name[end-inc]='\0'; }
                }
                char found_path[1024] = "";
                if (is_system) {
                    /* always pass system headers through as-is —
                       clang will handle them with full feature detection */
                    fprintf(out, "#include <%s>\n", name);
                } else {
                    FILE *inc_f = open_include(ctx, name, 0,
                                               cur_dir, found_path);
                    if (!inc_f) {
                        fprintf(out, "#include \"%s\"\n", name);
                    } else {
                        fprintf(out, "# 1 \"%s\" 1\n", found_path);
                        process_stream(ctx, inc_f, found_path, out, depth+1);
                        fclose(inc_f);
                        fprintf(out, "# %d \"%s\" 2\n", lineno, filename);
                    }
                }

            } else if (starts_with(s, "error")) {
                fprintf(stderr, "[VCP] %s:%d: #error %s\n",
                        filename, lineno, trim(s+5));
                ctx->errors++;
            } else if (starts_with(s, "warning")) {
                fprintf(stderr, "[VCP] %s:%d: #warning %s\n",
                        filename, lineno, trim(s+7));
            } else if (starts_with(s, "pragma")) {
                /* pass pragmas through */
                fprintf(out, "#pragma %s\n", trim(s+6));
            }
            /* #line — ignore */
        }
    }
    return ctx->errors;
}

int vcp_process_file(VCPCtx *ctx, const char *input,
                     const char *output) {
    FILE *in = fopen(input, "r");
    if (!in) { perror(input); return 1; }

    FILE *out = output ? fopen(output, "w") : stdout;
    if (!out) { perror(output); fclose(in); return 1; }

    fprintf(out, "# 1 \"%s\"\n", input);
    int ret = process_stream(ctx, in, input, out, 0);

    fclose(in);
    if (output) fclose(out);
    return ret;
}
