#include "codegen.h"
#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/BitWriter.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Transforms/PassBuilder.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Symbol table ───────────────────────────────────────────── */
#define SYM_MAX 1024

typedef struct {
    char       *name;
    LLVMValueRef value;
    LLVMTypeRef  type;
    int          is_alloca;   /* local var */
} Symbol;

typedef struct SymScope SymScope;
struct SymScope {
    Symbol     syms[SYM_MAX];
    int        count;
    SymScope  *parent;
};

static SymScope *scope_push(SymScope *parent) {
    SymScope *s = calloc(1, sizeof(SymScope));
    s->parent = parent;
    return s;
}
static SymScope *scope_pop(SymScope *s) {
    SymScope *p = s->parent;
    for (int i=0;i<s->count;i++) free(s->syms[i].name);
    free(s);
    return p;
}
static void scope_define(SymScope *s, const char *name,
                          LLVMValueRef val, LLVMTypeRef ty, int is_alloca) {
    s->syms[s->count].name = strdup(name);
    s->syms[s->count].value = val;
    s->syms[s->count].type = ty;
    s->syms[s->count].is_alloca = is_alloca;
    s->count++;
}
static Symbol *scope_lookup(SymScope *s, const char *name) {
    for (SymScope *sc=s; sc; sc=sc->parent)
        for (int i=0;i<sc->count;i++)
            if (!strcmp(sc->syms[i].name, name))
                return &sc->syms[i];
    return NULL;
}

/* ── Codegen context ─────────────────────────────────────────── */
struct CodegenCtx {
    LLVMContextRef  ctx;
    LLVMModuleRef   mod;
    LLVMBuilderRef  builder;
    VCCTarget       target;
    int             optimize;
    int             debug;
    SymScope       *scope;
    LLVMValueRef    cur_func;
    LLVMBasicBlockRef break_bb;
    LLVMBasicBlockRef cont_bb;
};

/* ── Type conversion C → LLVM ────────────────────────────────── */
static LLVMTypeRef llvm_type(CodegenCtx *cg, Type *t) {
    if (!t) return LLVMInt32TypeInContext(cg->ctx);
    switch (t->kind) {
        case TY_VOID:  return LLVMVoidTypeInContext(cg->ctx);
        case TY_BOOL:
        case TY_I8:
        case TY_U8:    return LLVMInt8TypeInContext(cg->ctx);
        case TY_I16:
        case TY_U16:   return LLVMInt16TypeInContext(cg->ctx);
        case TY_I32:
        case TY_U32:   return LLVMInt32TypeInContext(cg->ctx);
        case TY_I64:
        case TY_U64:   return LLVMInt64TypeInContext(cg->ctx);
        case TY_F32:   return LLVMFloatTypeInContext(cg->ctx);
        case TY_F64:   return LLVMDoubleTypeInContext(cg->ctx);
        case TY_PTR:
            if (!t->base || t->base->kind==TY_VOID)
                return LLVMPointerTypeInContext(cg->ctx, 0);
            return LLVMPointerTypeInContext(cg->ctx, 0);
        case TY_ARRAY: {
            LLVMTypeRef elem = llvm_type(cg, t->base);
            long sz = t->array_size > 0 ? t->array_size : 1;
            return LLVMArrayType2(elem, sz);
        }
        case TY_STRUCT:
        case TY_UNION: {
            /* opaque for now */
            LLVMTypeRef found = LLVMGetTypeByName2(cg->ctx,
                                    t->name ? t->name : "anon_struct");
            if (found) return found;
            LLVMTypeRef st = LLVMStructCreateNamed(cg->ctx,
                                    t->name ? t->name : "anon_struct");
            return st;
        }
        case TY_FUNC: {
            LLVMTypeRef ret = llvm_type(cg, t->base);
            LLVMTypeRef *ptypes = malloc(t->param_count * sizeof(LLVMTypeRef));
            for (int i=0;i<t->param_count;i++)
                ptypes[i]=llvm_type(cg, t->params[i]);
            LLVMTypeRef ft = LLVMFunctionType(ret, ptypes,
                                              t->param_count, t->is_variadic);
            free(ptypes);
            return ft;
        }
        default:
            return LLVMInt32TypeInContext(cg->ctx);
    }
}

static int type_is_float(Type *t) {
    return t && (t->kind==TY_F32||t->kind==TY_F64);
}
static int type_is_unsigned(Type *t) {
    return t && (t->kind==TY_U8||t->kind==TY_U16||
                 t->kind==TY_U32||t->kind==TY_U64);
}

/* ── Forward declaration ─────────────────────────────────────── */
static LLVMValueRef cg_expr(CodegenCtx *cg, ASTNode *n);
static void         cg_stmt(CodegenCtx *cg, ASTNode *n);

/* ── Expression code generation ─────────────────────────────── */
static LLVMValueRef cg_expr(CodegenCtx *cg, ASTNode *n) {
    if (!n) return NULL;

    switch (n->expr.kind) {
    case EXPR_INT_LIT:
        return LLVMConstInt(LLVMInt32TypeInContext(cg->ctx),
                            n->expr.int_val, 1);
    case EXPR_FLOAT_LIT:
        return LLVMConstReal(LLVMDoubleTypeInContext(cg->ctx),
                             n->expr.float_val);
    case EXPR_CHAR_LIT:
        return LLVMConstInt(LLVMInt8TypeInContext(cg->ctx),
                            n->expr.int_val, 0);
    case EXPR_NULL_LIT:
        return LLVMConstPointerNull(
            LLVMPointerTypeInContext(cg->ctx,0));
    case EXPR_STR_LIT: {
        LLVMValueRef gs = LLVMBuildGlobalStringPtr(cg->builder,
                            n->expr.str_val, ".str");
        return gs;
    }
    case EXPR_IDENT: {
        Symbol *sym = scope_lookup(cg->scope, n->expr.ident);
        if (!sym) {
            /* might be a function – try to find in module */
            LLVMValueRef fn = LLVMGetNamedFunction(cg->mod, n->expr.ident);
            if (fn) return fn;
            /* might be a global */
            LLVMValueRef gv = LLVMGetNamedGlobal(cg->mod, n->expr.ident);
            if (gv) return LLVMBuildLoad2(cg->builder,
                        LLVMGlobalGetValueType(gv), gv, n->expr.ident);
            fprintf(stderr, "codegen: undefined symbol '%s'\n",
                    n->expr.ident);
            return LLVMConstInt(LLVMInt32TypeInContext(cg->ctx),0,0);
        }
        if (sym->is_alloca)
            return LLVMBuildLoad2(cg->builder, sym->type,
                                  sym->value, n->expr.ident);
        return sym->value;
    }
    case EXPR_ASSIGN: {
        LLVMValueRef rhs = cg_expr(cg, n->expr.right);
        /* find lvalue */
        ASTNode *lhs_node = n->expr.left;
        if (lhs_node->expr.kind == EXPR_IDENT) {
            Symbol *sym = scope_lookup(cg->scope, lhs_node->expr.ident);
            if (sym && sym->is_alloca) {
                const char *op = n->expr.op;
                if (strcmp(op,"=")!=0 && rhs) {
                    LLVMValueRef cur_val = LLVMBuildLoad2(cg->builder,
                                             sym->type, sym->value, "load");
                    if      (!strcmp(op,"+="))  rhs=LLVMBuildAdd(cg->builder,cur_val,rhs,"add");
                    else if (!strcmp(op,"-="))  rhs=LLVMBuildSub(cg->builder,cur_val,rhs,"sub");
                    else if (!strcmp(op,"*="))  rhs=LLVMBuildMul(cg->builder,cur_val,rhs,"mul");
                    else if (!strcmp(op,"/="))  rhs=LLVMBuildSDiv(cg->builder,cur_val,rhs,"div");
                    else if (!strcmp(op,"%="))  rhs=LLVMBuildSRem(cg->builder,cur_val,rhs,"rem");
                    else if (!strcmp(op,"&="))  rhs=LLVMBuildAnd(cg->builder,cur_val,rhs,"and");
                    else if (!strcmp(op,"|="))  rhs=LLVMBuildOr(cg->builder,cur_val,rhs,"or");
                    else if (!strcmp(op,"^="))  rhs=LLVMBuildXor(cg->builder,cur_val,rhs,"xor");
                }
                LLVMBuildStore(cg->builder, rhs, sym->value);
                return rhs;
            }
        } else if (lhs_node->expr.kind==EXPR_DEREF) {
            LLVMValueRef ptr=cg_expr(cg, lhs_node->expr.left);
            LLVMBuildStore(cg->builder, rhs, ptr);
            return rhs;
        } else if (lhs_node->expr.kind==EXPR_INDEX) {
            /* array[index] = rhs */
            /* compute pointer */
            LLVMValueRef base=NULL;
            Symbol *sym=scope_lookup(cg->scope,
                lhs_node->expr.base_expr->expr.ident);
            if (sym) base=sym->value;
            LLVMValueRef idx=cg_expr(cg, lhs_node->expr.left);
            if (base && idx) {
                LLVMValueRef gep=LLVMBuildInBoundsGEP2(cg->builder,
                    LLVMInt32TypeInContext(cg->ctx), base, &idx, 1, "gep");
                LLVMBuildStore(cg->builder, rhs, gep);
            }
            return rhs;
        }
        return rhs;
    }
    case EXPR_BINARY: {
        LLVMValueRef l = cg_expr(cg, n->expr.left);
        LLVMValueRef r = cg_expr(cg, n->expr.right);
        if (!l || !r) return NULL;
        const char *op = n->expr.op;
        int is_fp = LLVMGetTypeKind(LLVMTypeOf(l))==LLVMFloatTypeKind ||
                    LLVMGetTypeKind(LLVMTypeOf(l))==LLVMDoubleTypeKind;
        if (!strcmp(op,"+"))  return is_fp?LLVMBuildFAdd(cg->builder,l,r,"fadd"):LLVMBuildAdd(cg->builder,l,r,"add");
        if (!strcmp(op,"-"))  return is_fp?LLVMBuildFSub(cg->builder,l,r,"fsub"):LLVMBuildSub(cg->builder,l,r,"sub");
        if (!strcmp(op,"*"))  return is_fp?LLVMBuildFMul(cg->builder,l,r,"fmul"):LLVMBuildMul(cg->builder,l,r,"mul");
        if (!strcmp(op,"/"))  return is_fp?LLVMBuildFDiv(cg->builder,l,r,"fdiv"):LLVMBuildSDiv(cg->builder,l,r,"sdiv");
        if (!strcmp(op,"%"))  return LLVMBuildSRem(cg->builder,l,r,"srem");
        if (!strcmp(op,"&"))  return LLVMBuildAnd(cg->builder,l,r,"and");
        if (!strcmp(op,"|"))  return LLVMBuildOr(cg->builder,l,r,"or");
        if (!strcmp(op,"^"))  return LLVMBuildXor(cg->builder,l,r,"xor");
        if (!strcmp(op,"<<")) return LLVMBuildShl(cg->builder,l,r,"shl");
        if (!strcmp(op,">>")) return LLVMBuildAShr(cg->builder,l,r,"ashr");
        /* comparisons → i1, extend to i32 */
        LLVMValueRef cmp=NULL;
        if (is_fp) {
            if      (!strcmp(op,"==")) cmp=LLVMBuildFCmp(cg->builder,LLVMRealOEQ,l,r,"fcmp");
            else if (!strcmp(op,"!=")) cmp=LLVMBuildFCmp(cg->builder,LLVMRealONE,l,r,"fcmp");
            else if (!strcmp(op,"<"))  cmp=LLVMBuildFCmp(cg->builder,LLVMRealOLT,l,r,"fcmp");
            else if (!strcmp(op,">"))  cmp=LLVMBuildFCmp(cg->builder,LLVMRealOGT,l,r,"fcmp");
            else if (!strcmp(op,"<=")) cmp=LLVMBuildFCmp(cg->builder,LLVMRealOLE,l,r,"fcmp");
            else if (!strcmp(op,">=")) cmp=LLVMBuildFCmp(cg->builder,LLVMRealOGE,l,r,"fcmp");
        } else {
            if      (!strcmp(op,"==")) cmp=LLVMBuildICmp(cg->builder,LLVMIntEQ,l,r,"icmp");
            else if (!strcmp(op,"!=")) cmp=LLVMBuildICmp(cg->builder,LLVMIntNE,l,r,"icmp");
            else if (!strcmp(op,"<"))  cmp=LLVMBuildICmp(cg->builder,LLVMIntSLT,l,r,"icmp");
            else if (!strcmp(op,">"))  cmp=LLVMBuildICmp(cg->builder,LLVMIntSGT,l,r,"icmp");
            else if (!strcmp(op,"<=")) cmp=LLVMBuildICmp(cg->builder,LLVMIntSLE,l,r,"icmp");
            else if (!strcmp(op,">=")) cmp=LLVMBuildICmp(cg->builder,LLVMIntSGE,l,r,"icmp");
        }
        if (cmp) return LLVMBuildZExt(cg->builder,cmp,
                                      LLVMInt32TypeInContext(cg->ctx),"zext");
        /* && || */
        if (!strcmp(op,"&&")) {
            LLVMValueRef lb=LLVMBuildICmp(cg->builder,LLVMIntNE,l,
                LLVMConstInt(LLVMTypeOf(l),0,0),"l");
            LLVMValueRef rb=LLVMBuildICmp(cg->builder,LLVMIntNE,r,
                LLVMConstInt(LLVMTypeOf(r),0,0),"r");
            LLVMValueRef res=LLVMBuildAnd(cg->builder,lb,rb,"land");
            return LLVMBuildZExt(cg->builder,res,
                                 LLVMInt32TypeInContext(cg->ctx),"zext");
        }
        if (!strcmp(op,"||")) {
            LLVMValueRef lb=LLVMBuildICmp(cg->builder,LLVMIntNE,l,
                LLVMConstInt(LLVMTypeOf(l),0,0),"l");
            LLVMValueRef rb=LLVMBuildICmp(cg->builder,LLVMIntNE,r,
                LLVMConstInt(LLVMTypeOf(r),0,0),"r");
            LLVMValueRef res=LLVMBuildOr(cg->builder,lb,rb,"lor");
            return LLVMBuildZExt(cg->builder,res,
                                 LLVMInt32TypeInContext(cg->ctx),"zext");
        }
        return l;
    }
    case EXPR_UNARY: {
        LLVMValueRef v=cg_expr(cg,n->expr.left);
        if (!v) return NULL;
        const char *op=n->expr.op;
        if (!strcmp(op,"-")) {
            if (LLVMGetTypeKind(LLVMTypeOf(v))==LLVMFloatTypeKind ||
                LLVMGetTypeKind(LLVMTypeOf(v))==LLVMDoubleTypeKind)
                return LLVMBuildFNeg(cg->builder,v,"fneg");
            return LLVMBuildNeg(cg->builder,v,"neg");
        }
        if (!strcmp(op,"!")) {
            LLVMValueRef z=LLVMConstInt(LLVMTypeOf(v),0,0);
            LLVMValueRef cmp=LLVMBuildICmp(cg->builder,LLVMIntEQ,v,z,"not");
            return LLVMBuildZExt(cg->builder,cmp,
                                 LLVMInt32TypeInContext(cg->ctx),"zext");
        }
        if (!strcmp(op,"~")) return LLVMBuildNot(cg->builder,v,"bnot");
        return v;
    }
    case EXPR_ADDR: {
        ASTNode *sub=n->expr.left;
        if (sub->expr.kind==EXPR_IDENT) {
            Symbol *sym=scope_lookup(cg->scope,sub->expr.ident);
            if (sym && sym->is_alloca) return sym->value;
        }
        return cg_expr(cg, sub);
    }
    case EXPR_DEREF: {
        LLVMValueRef ptr=cg_expr(cg,n->expr.left);
        if (!ptr) return NULL;
        return LLVMBuildLoad2(cg->builder,
                   LLVMInt32TypeInContext(cg->ctx), ptr, "deref");
    }
    case EXPR_INDEX: {
        ASTNode *base_e=n->expr.base_expr;
        Symbol *sym=scope_lookup(cg->scope,base_e->expr.ident);
        LLVMValueRef base=sym?sym->value:cg_expr(cg,base_e);
        LLVMValueRef idx=cg_expr(cg,n->expr.left);
        LLVMTypeRef  elem=LLVMInt32TypeInContext(cg->ctx);
        LLVMValueRef gep=LLVMBuildInBoundsGEP2(cg->builder,
                              elem,base,&idx,1,"idx");
        return LLVMBuildLoad2(cg->builder,elem,gep,"load");
    }
    case EXPR_CALL: {
        ASTNode *callee_n=n->expr.callee;
        LLVMValueRef fn=NULL;
        LLVMTypeRef  ft=NULL;

        if (callee_n->expr.kind==EXPR_IDENT) {
            fn=LLVMGetNamedFunction(cg->mod,callee_n->expr.ident);
            if (!fn) {
                /* auto-declare as variadic i32 func */
                LLVMTypeRef vft=LLVMFunctionType(
                    LLVMInt32TypeInContext(cg->ctx), NULL, 0, 1);
                fn=LLVMAddFunction(cg->mod,callee_n->expr.ident,vft);
                ft=vft;
            } else {
                ft=LLVMGlobalGetValueType(fn);
            }
        }
        if (!fn) return NULL;
        if (!ft) ft=LLVMGlobalGetValueType(fn);

        LLVMValueRef *args=malloc(n->expr.arg_count*sizeof(LLVMValueRef));
        for (int i=0;i<n->expr.arg_count;i++)
            args[i]=cg_expr(cg,n->expr.args[i]);
        LLVMValueRef result=LLVMBuildCall2(cg->builder,ft,fn,
                                args,n->expr.arg_count,"call");
        free(args);
        return result;
    }
    case EXPR_INC_PRE: {
        ASTNode *sub=n->expr.left;
        Symbol *sym=scope_lookup(cg->scope,sub->expr.ident);
        if (sym && sym->is_alloca) {
            LLVMValueRef v=LLVMBuildLoad2(cg->builder,sym->type,sym->value,"load");
            LLVMValueRef one=LLVMConstInt(sym->type,1,0);
            LLVMValueRef nv=LLVMBuildAdd(cg->builder,v,one,"inc");
            LLVMBuildStore(cg->builder,nv,sym->value);
            return nv;
        }
        return NULL;
    }
    case EXPR_DEC_PRE: {
        ASTNode *sub=n->expr.left;
        Symbol *sym=scope_lookup(cg->scope,sub->expr.ident);
        if (sym && sym->is_alloca) {
            LLVMValueRef v=LLVMBuildLoad2(cg->builder,sym->type,sym->value,"load");
            LLVMValueRef one=LLVMConstInt(sym->type,1,0);
            LLVMValueRef nv=LLVMBuildSub(cg->builder,v,one,"dec");
            LLVMBuildStore(cg->builder,nv,sym->value);
            return nv;
        }
        return NULL;
    }
    case EXPR_INC_POST: {
        ASTNode *sub=n->expr.left;
        Symbol *sym=scope_lookup(cg->scope,sub->expr.ident);
        if (sym && sym->is_alloca) {
            LLVMValueRef v=LLVMBuildLoad2(cg->builder,sym->type,sym->value,"load");
            LLVMValueRef one=LLVMConstInt(sym->type,1,0);
            LLVMValueRef nv=LLVMBuildAdd(cg->builder,v,one,"inc");
            LLVMBuildStore(cg->builder,nv,sym->value);
            return v; /* return old value */
        }
        return NULL;
    }
    case EXPR_DEC_POST: {
        ASTNode *sub=n->expr.left;
        Symbol *sym=scope_lookup(cg->scope,sub->expr.ident);
        if (sym && sym->is_alloca) {
            LLVMValueRef v=LLVMBuildLoad2(cg->builder,sym->type,sym->value,"load");
            LLVMValueRef one=LLVMConstInt(sym->type,1,0);
            LLVMValueRef nv=LLVMBuildSub(cg->builder,v,one,"dec");
            LLVMBuildStore(cg->builder,nv,sym->value);
            return v;
        }
        return NULL;
    }
    case EXPR_CAST: {
        LLVMValueRef v=cg_expr(cg,n->expr.left);
        LLVMTypeRef dst=llvm_type(cg,n->expr.cast_type);
        if (!v) return NULL;
        LLVMTypeKind sk=LLVMGetTypeKind(LLVMTypeOf(v));
        LLVMTypeKind dk=LLVMGetTypeKind(dst);
        if (sk==dk) return v;
        if (dk==LLVMPointerTypeKind)
            return LLVMBuildIntToPtr(cg->builder,v,dst,"i2p");
        if (sk==LLVMPointerTypeKind)
            return LLVMBuildPtrToInt(cg->builder,v,dst,"p2i");
        if ((sk==LLVMFloatTypeKind||sk==LLVMDoubleTypeKind) &&
            (dk==LLVMIntegerTypeKind))
            return LLVMBuildFPToSI(cg->builder,v,dst,"fp2si");
        if ((sk==LLVMIntegerTypeKind) &&
            (dk==LLVMFloatTypeKind||dk==LLVMDoubleTypeKind))
            return LLVMBuildSIToFP(cg->builder,v,dst,"si2fp");
        if (sk==LLVMIntegerTypeKind && dk==LLVMIntegerTypeKind) {
            unsigned sb=LLVMGetIntTypeWidth(LLVMTypeOf(v));
            unsigned db=LLVMGetIntTypeWidth(dst);
            if (db>sb) return LLVMBuildSExt(cg->builder,v,dst,"sext");
            return LLVMBuildTrunc(cg->builder,v,dst,"trunc");
        }
        return v;
    }
    case EXPR_SIZEOF_TYPE: {
        LLVMTypeRef ty=llvm_type(cg,n->expr.cast_type);
        LLVMValueRef sz=LLVMSizeOf(ty);
        return LLVMBuildTrunc(cg->builder,sz,
                              LLVMInt32TypeInContext(cg->ctx),"sizeof");
    }
    case EXPR_SIZEOF_EXPR: {
        /* evaluate type only */
        LLVMTypeRef ty=LLVMInt32TypeInContext(cg->ctx);
        (void)ty;
        return LLVMConstInt(LLVMInt32TypeInContext(cg->ctx),4,0);
    }
    case EXPR_TERNARY: {
        LLVMValueRef cond_v=cg_expr(cg,n->expr.cond);
        LLVMValueRef zero=LLVMConstInt(LLVMTypeOf(cond_v),0,0);
        LLVMValueRef cond_i1=LLVMBuildICmp(cg->builder,LLVMIntNE,
                                            cond_v,zero,"cond");
        LLVMBasicBlockRef then_bb=LLVMAppendBasicBlockInContext(cg->ctx,
                                    cg->cur_func,"tern.then");
        LLVMBasicBlockRef else_bb=LLVMAppendBasicBlockInContext(cg->ctx,
                                    cg->cur_func,"tern.else");
        LLVMBasicBlockRef merge_bb=LLVMAppendBasicBlockInContext(cg->ctx,
                                    cg->cur_func,"tern.merge");
        LLVMBuildCondBr(cg->builder,cond_i1,then_bb,else_bb);
        LLVMPositionBuilderAtEnd(cg->builder,then_bb);
        LLVMValueRef tv=cg_expr(cg,n->expr.left);
        LLVMBuildBr(cg->builder,merge_bb);
        LLVMBasicBlockRef then_end=LLVMGetInsertBlock(cg->builder);
        LLVMPositionBuilderAtEnd(cg->builder,else_bb);
        LLVMValueRef ev=cg_expr(cg,n->expr.right);
        LLVMBuildBr(cg->builder,merge_bb);
        LLVMBasicBlockRef else_end=LLVMGetInsertBlock(cg->builder);
        LLVMPositionBuilderAtEnd(cg->builder,merge_bb);
        LLVMValueRef phi=LLVMBuildPhi(cg->builder,LLVMTypeOf(tv),"phi");
        LLVMValueRef vals[2]={tv,ev};
        LLVMBasicBlockRef bbs[2]={then_end,else_end};
        LLVMAddIncoming(phi,vals,bbs,2);
        return phi;
    }
    default:
        return NULL;
    }
}

/* ── Statement code generation ───────────────────────────────── */
static void cg_stmt(CodegenCtx *cg, ASTNode *n) {
    if (!n) return;

    if (n->category == NODE_DECL) {
        /* local variable declaration */
        Type *ty = n->decl.type;
        LLVMTypeRef lty = llvm_type(cg, ty);
        LLVMValueRef alloca_val = LLVMBuildAlloca(cg->builder, lty,
                                      n->decl.name ? n->decl.name : "tmp");
        if (n->decl.name)
            scope_define(cg->scope, n->decl.name, alloca_val, lty, 1);
        if (n->decl.init) {
            LLVMValueRef init_v = cg_expr(cg, n->decl.init);
            if (init_v) LLVMBuildStore(cg->builder, init_v, alloca_val);
        }
        return;
    }

    /* STMT */
    switch (n->stmt.kind) {
    case STMT_EXPR:
        cg_expr(cg, n->stmt.expr);
        break;

    case STMT_BLOCK:
        cg->scope = scope_push(cg->scope);
        for (int i=0;i<n->stmt.stmt_count;i++)
            cg_stmt(cg, n->stmt.stmts[i]);
        cg->scope = scope_pop(cg->scope);
        break;

    case STMT_RETURN: {
        if (n->stmt.expr) {
            LLVMValueRef rv = cg_expr(cg, n->stmt.expr);
            LLVMTypeRef ret_ty = LLVMGetReturnType(
                LLVMGlobalGetValueType(cg->cur_func));
            /* coerce if needed */
            if (rv && LLVMTypeOf(rv)!=ret_ty) {
                LLVMTypeKind sk=LLVMGetTypeKind(LLVMTypeOf(rv));
                LLVMTypeKind dk=LLVMGetTypeKind(ret_ty);
                if (sk==LLVMIntegerTypeKind && dk==LLVMIntegerTypeKind) {
                    unsigned sb=LLVMGetIntTypeWidth(LLVMTypeOf(rv));
                    unsigned db=LLVMGetIntTypeWidth(ret_ty);
                    if (db>sb) rv=LLVMBuildSExt(cg->builder,rv,ret_ty,"sext");
                    else if (db<sb) rv=LLVMBuildTrunc(cg->builder,rv,ret_ty,"trunc");
                }
            }
            LLVMBuildRet(cg->builder, rv);
        } else {
            LLVMBuildRetVoid(cg->builder);
        }
        /* add unreachable block so we can continue appending */
        LLVMBasicBlockRef dead=LLVMAppendBasicBlockInContext(cg->ctx,
                                   cg->cur_func,"after.ret");
        LLVMPositionBuilderAtEnd(cg->builder,dead);
        break;
    }
    case STMT_IF: {
        LLVMValueRef cond_v=cg_expr(cg,n->stmt.cond);
        LLVMValueRef zero=LLVMConstInt(LLVMTypeOf(cond_v),0,0);
        LLVMValueRef cond_i1=LLVMBuildICmp(cg->builder,LLVMIntNE,
                                            cond_v,zero,"if.cond");
        LLVMBasicBlockRef then_bb=LLVMAppendBasicBlockInContext(cg->ctx,
                                    cg->cur_func,"if.then");
        LLVMBasicBlockRef else_bb=n->stmt.else_body?
            LLVMAppendBasicBlockInContext(cg->ctx,cg->cur_func,"if.else"):NULL;
        LLVMBasicBlockRef merge_bb=LLVMAppendBasicBlockInContext(cg->ctx,
                                    cg->cur_func,"if.merge");
        LLVMBuildCondBr(cg->builder,cond_i1,then_bb,
                        else_bb?else_bb:merge_bb);
        LLVMPositionBuilderAtEnd(cg->builder,then_bb);
        cg_stmt(cg,n->stmt.body);
        if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(cg->builder)))
            LLVMBuildBr(cg->builder,merge_bb);
        if (else_bb) {
            LLVMPositionBuilderAtEnd(cg->builder,else_bb);
            cg_stmt(cg,n->stmt.else_body);
            if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(cg->builder)))
                LLVMBuildBr(cg->builder,merge_bb);
        }
        LLVMPositionBuilderAtEnd(cg->builder,merge_bb);
        break;
    }
    case STMT_WHILE: {
        LLVMBasicBlockRef cond_bb=LLVMAppendBasicBlockInContext(cg->ctx,
                                    cg->cur_func,"while.cond");
        LLVMBasicBlockRef body_bb=LLVMAppendBasicBlockInContext(cg->ctx,
                                    cg->cur_func,"while.body");
        LLVMBasicBlockRef exit_bb=LLVMAppendBasicBlockInContext(cg->ctx,
                                    cg->cur_func,"while.exit");
        LLVMBuildBr(cg->builder,cond_bb);
        LLVMPositionBuilderAtEnd(cg->builder,cond_bb);
        LLVMValueRef cv=cg_expr(cg,n->stmt.cond);
        LLVMValueRef z=LLVMConstInt(LLVMTypeOf(cv),0,0);
        LLVMValueRef ci=LLVMBuildICmp(cg->builder,LLVMIntNE,cv,z,"wc");
        LLVMBuildCondBr(cg->builder,ci,body_bb,exit_bb);
        LLVMPositionBuilderAtEnd(cg->builder,body_bb);
        LLVMBasicBlockRef save_brk=cg->break_bb, save_cnt=cg->cont_bb;
        cg->break_bb=exit_bb; cg->cont_bb=cond_bb;
        cg_stmt(cg,n->stmt.body);
        cg->break_bb=save_brk; cg->cont_bb=save_cnt;
        if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(cg->builder)))
            LLVMBuildBr(cg->builder,cond_bb);
        LLVMPositionBuilderAtEnd(cg->builder,exit_bb);
        break;
    }
    case STMT_FOR: {
        cg->scope=scope_push(cg->scope);
        if (n->stmt.init) cg_stmt(cg,n->stmt.init);
        LLVMBasicBlockRef cond_bb=LLVMAppendBasicBlockInContext(cg->ctx,
                                    cg->cur_func,"for.cond");
        LLVMBasicBlockRef body_bb=LLVMAppendBasicBlockInContext(cg->ctx,
                                    cg->cur_func,"for.body");
        LLVMBasicBlockRef step_bb=LLVMAppendBasicBlockInContext(cg->ctx,
                                    cg->cur_func,"for.step");
        LLVMBasicBlockRef exit_bb=LLVMAppendBasicBlockInContext(cg->ctx,
                                    cg->cur_func,"for.exit");
        LLVMBuildBr(cg->builder,cond_bb);
        LLVMPositionBuilderAtEnd(cg->builder,cond_bb);
        if (n->stmt.cond) {
            LLVMValueRef cv=cg_expr(cg,n->stmt.cond);
            LLVMValueRef z=LLVMConstInt(LLVMTypeOf(cv),0,0);
            LLVMValueRef ci=LLVMBuildICmp(cg->builder,LLVMIntNE,cv,z,"fc");
            LLVMBuildCondBr(cg->builder,ci,body_bb,exit_bb);
        } else LLVMBuildBr(cg->builder,body_bb);
        LLVMPositionBuilderAtEnd(cg->builder,body_bb);
        LLVMBasicBlockRef save_brk=cg->break_bb, save_cnt=cg->cont_bb;
        cg->break_bb=exit_bb; cg->cont_bb=step_bb;
        cg_stmt(cg,n->stmt.body);
        cg->break_bb=save_brk; cg->cont_bb=save_cnt;
        if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(cg->builder)))
            LLVMBuildBr(cg->builder,step_bb);
        LLVMPositionBuilderAtEnd(cg->builder,step_bb);
        if (n->stmt.step) cg_expr(cg,n->stmt.step);
        LLVMBuildBr(cg->builder,cond_bb);
        LLVMPositionBuilderAtEnd(cg->builder,exit_bb);
        cg->scope=scope_pop(cg->scope);
        break;
    }
    case STMT_DO_WHILE: {
        LLVMBasicBlockRef body_bb=LLVMAppendBasicBlockInContext(cg->ctx,
                                    cg->cur_func,"do.body");
        LLVMBasicBlockRef cond_bb=LLVMAppendBasicBlockInContext(cg->ctx,
                                    cg->cur_func,"do.cond");
        LLVMBasicBlockRef exit_bb=LLVMAppendBasicBlockInContext(cg->ctx,
                                    cg->cur_func,"do.exit");
        LLVMBuildBr(cg->builder,body_bb);
        LLVMPositionBuilderAtEnd(cg->builder,body_bb);
        LLVMBasicBlockRef save_brk=cg->break_bb,save_cnt=cg->cont_bb;
        cg->break_bb=exit_bb; cg->cont_bb=cond_bb;
        cg_stmt(cg,n->stmt.body);
        cg->break_bb=save_brk; cg->cont_bb=save_cnt;
        if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(cg->builder)))
            LLVMBuildBr(cg->builder,cond_bb);
        LLVMPositionBuilderAtEnd(cg->builder,cond_bb);
        LLVMValueRef cv=cg_expr(cg,n->stmt.cond);
        LLVMValueRef z=LLVMConstInt(LLVMTypeOf(cv),0,0);
        LLVMValueRef ci=LLVMBuildICmp(cg->builder,LLVMIntNE,cv,z,"dc");
        LLVMBuildCondBr(cg->builder,ci,body_bb,exit_bb);
        LLVMPositionBuilderAtEnd(cg->builder,exit_bb);
        break;
    }
    case STMT_BREAK:
        if (cg->break_bb)
            LLVMBuildBr(cg->builder,cg->break_bb);
        break;
    case STMT_CONTINUE:
        if (cg->cont_bb)
            LLVMBuildBr(cg->builder,cg->cont_bb);
        break;
    case STMT_NULL: break;
    default: break;
    }
}

/* ── Top-level codegen ───────────────────────────────────────── */

static void cg_global_decl(CodegenCtx *cg, ASTNode *n) {
    if (n->category != NODE_DECL) return;

    /* Function */
    if (n->decl.type && n->decl.type->kind == TY_FUNC) {
        LLVMTypeRef ft = llvm_type(cg, n->decl.type);
        LLVMValueRef fn = LLVMGetNamedFunction(cg->mod, n->decl.name);
        if (!fn) fn = LLVMAddFunction(cg->mod, n->decl.name, ft);

        if (!n->decl.body) return; /* prototype */

        /* build function body */
        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(
                                      cg->ctx, fn, "entry");
        LLVMPositionBuilderAtEnd(cg->builder, entry);

        cg->cur_func = fn;
        cg->scope    = scope_push(cg->scope);

        /* bind params */
        int pcount = LLVMCountParams(fn);
        for (int i=0; i<pcount && i<n->decl.type->param_count; i++) {
            LLVMValueRef pval  = LLVMGetParam(fn, i);
            LLVMTypeRef  pty   = llvm_type(cg, n->decl.type->params[i]);
            const char  *pname = n->decl.param_names &&
                                 n->decl.param_names[i] ?
                                 n->decl.param_names[i] : "arg";
            LLVMSetValueName(pval, pname);
            LLVMValueRef alloca=LLVMBuildAlloca(cg->builder,pty,pname);
            LLVMBuildStore(cg->builder,pval,alloca);
            scope_define(cg->scope,pname,alloca,pty,1);
        }

        cg_stmt(cg, n->decl.body);

        /* ensure terminator */
        if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(cg->builder))) {
            LLVMTypeRef ret=LLVMGetReturnType(ft);
            if (LLVMGetTypeKind(ret)==LLVMVoidTypeKind)
                LLVMBuildRetVoid(cg->builder);
            else
                LLVMBuildRet(cg->builder,LLVMConstInt(ret,0,0));
        }

        cg->scope = scope_pop(cg->scope);
        return;
    }

    /* Global variable */
    if (n->decl.name) {
        LLVMTypeRef gty = llvm_type(cg, n->decl.type);
        LLVMValueRef gv = LLVMAddGlobal(cg->mod, gty, n->decl.name);
        if (n->decl.is_extern) {
            LLVMSetLinkage(gv, LLVMExternalLinkage);
            LLVMSetInitializer(gv, LLVMGetUndef(gty));
        } else {
            LLVMSetInitializer(gv, LLVMConstNull(gty));
        }
        scope_define(cg->scope, n->decl.name, gv, gty, 0);
    }
}

/* ── Public API ──────────────────────────────────────────────── */

CodegenCtx *codegen_create(const char *module_name,
                            VCCTarget target, int opt, int dbg) {
    LLVMInitializeAllTargetInfos();
    LLVMInitializeAllTargets();
    LLVMInitializeAllTargetMCs();
    LLVMInitializeAllAsmPrinters();
    LLVMInitializeAllAsmParsers();

    CodegenCtx *cg = calloc(1, sizeof(CodegenCtx));
    cg->ctx     = LLVMContextCreate();
    cg->mod     = LLVMModuleCreateWithNameInContext(module_name, cg->ctx);
    cg->builder = LLVMCreateBuilderInContext(cg->ctx);
    cg->target  = target;
    cg->optimize= opt;
    cg->debug   = dbg;
    cg->scope   = scope_push(NULL); /* global scope */
    return cg;
}

int codegen_emit(CodegenCtx *cg, TranslationUnit *tu,
                 const char *output_file, int emit_ir) {
    /* set target triple */
    const VCCTargetInfo *ti = vcc_get_target_info(cg->target);
    if (ti && ti->triple)
        LLVMSetTarget(cg->mod, ti->triple);

    /* generate all top-level declarations */
    for (int i=0; i<tu->count; i++)
        cg_global_decl(cg, tu->decls[i]);

    /* verify */
    char *err = NULL;
    if (LLVMVerifyModule(cg->mod, LLVMPrintMessageAction, &err)) {
        fprintf(stderr, "[codegen] verification error: %s\n", err);
        LLVMDisposeMessage(err);
        return 1;
    }
    LLVMDisposeMessage(err);

    /* emit */
    if (emit_ir) {
        if (LLVMPrintModuleToFile(cg->mod, output_file, &err)) {
            fprintf(stderr, "[codegen] write error: %s\n", err);
            LLVMDisposeMessage(err);
            return 1;
        }
    } else {
        if (LLVMWriteBitcodeToFile(cg->mod, output_file)) {
            fprintf(stderr, "[codegen] bitcode write error\n");
            return 1;
        }
    }
    return 0;
}

void codegen_free(CodegenCtx *cg) {
    if (!cg) return;
    while (cg->scope) cg->scope = scope_pop(cg->scope);
    LLVMDisposeBuilder(cg->builder);
    LLVMDisposeModule(cg->mod);
    LLVMContextDispose(cg->ctx);
    free(cg);
}
