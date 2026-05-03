#ifndef VCP_H
#define VCP_H

#include <stddef.h>

#define VCP_MAX_MACROS     4096
#define VCP_MAX_INCLUDES   256
#define VCP_MAX_DEPTH      64
#define VCP_LINE_MAX       16384

typedef struct {
    char *name;
    char *params[64];
    int   param_count;
    int   is_func_like;
    char *body;
} VCPMacro;

typedef struct VCPCtx VCPCtx;

VCPCtx *vcp_create(void);
void    vcp_free(VCPCtx *ctx);
void    vcp_add_include_dir(VCPCtx *ctx, const char *dir);
void    vcp_define(VCPCtx *ctx, const char *name, const char *val);
void    vcp_undefine(VCPCtx *ctx, const char *name);
int     vcp_process_file(VCPCtx *ctx, const char *input,
                         const char *output);

#endif /* VCP_H */
