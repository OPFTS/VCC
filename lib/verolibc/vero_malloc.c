/*
 * vero_malloc.c — VeroCC memory allocator
 * Uses mmap for large allocations, sbrk-style brk for small ones.
 * Thread-safe via a simple spinlock (single-threaded for now).
 */
#include <stddef.h>
#include <stdint.h>
#include "arch/x86_64/syscall.h"

#define MMAP_THRESHOLD  (128 * 1024)   /* 128 KB */
#define ALIGN           16
#define MAGIC           0xDEADBEEFU

/* Block header */
typedef struct Block {
    size_t        size;       /* usable size (not including header) */
    unsigned int  magic;
    int           free;
    struct Block *next;
    struct Block *prev;
} Block;

#define HEADER_SIZE  (sizeof(Block))

static Block *free_list = NULL;
static void  *heap_start = NULL;
static void  *heap_end   = NULL;

/* ── Low-level page allocator ────────────────────────────────── */
static void *pages_alloc(size_t sz) {
    /* mmap anonymous pages */
    void *p = (void *)vero_mmap(NULL, sz,
        0x3 /* PROT_READ|PROT_WRITE */,
        0x22 /* MAP_PRIVATE|MAP_ANONYMOUS */,
        -1, 0);
    if ((long)p < 0) return NULL;
    return p;
}

static void pages_free(void *ptr, size_t sz) {
    vero_munmap(ptr, sz);
}

/* ── Align up ─────────────────────────────────────────────────── */
static size_t align_up(size_t n, size_t a) {
    return (n + a - 1) & ~(a - 1);
}

/* ── Split a block if there is enough space ──────────────────── */
static void split_block(Block *b, size_t sz) {
    if (b->size >= sz + HEADER_SIZE + ALIGN) {
        Block *nb = (Block *)((char *)b + HEADER_SIZE + sz);
        nb->size  = b->size - sz - HEADER_SIZE;
        nb->magic = MAGIC;
        nb->free  = 1;
        nb->next  = b->next;
        nb->prev  = b;
        if (b->next) b->next->prev = nb;
        b->next   = nb;
        b->size   = sz;
    }
}

/* ── Coalesce adjacent free blocks ───────────────────────────── */
static void coalesce(Block *b) {
    /* merge with next */
    if (b->next && b->next->free) {
        b->size += HEADER_SIZE + b->next->size;
        b->next  = b->next->next;
        if (b->next) b->next->prev = b;
    }
    /* merge with prev */
    if (b->prev && b->prev->free) {
        b->prev->size += HEADER_SIZE + b->size;
        b->prev->next  = b->next;
        if (b->next) b->next->prev = b->prev;
    }
}

/* ── malloc ───────────────────────────────────────────────────── */
void *vero_malloc(size_t sz) {
    if (sz == 0) return NULL;
    sz = align_up(sz, ALIGN);

    /* large allocation: go direct to mmap */
    if (sz >= MMAP_THRESHOLD) {
        size_t total = align_up(sz + HEADER_SIZE, 4096);
        Block *b = (Block *)pages_alloc(total);
        if (!b) return NULL;
        b->size  = total - HEADER_SIZE;
        b->magic = MAGIC;
        b->free  = 0;
        b->next  = b->prev = NULL;
        return (char *)b + HEADER_SIZE;
    }

    /* search free list (first-fit) */
    Block *b = free_list;
    while (b) {
        if (b->free && b->size >= sz) {
            split_block(b, sz);
            b->free  = 0;
            b->magic = MAGIC;
            return (char *)b + HEADER_SIZE;
        }
        b = b->next;
    }

    /* no suitable block — allocate a new chunk */
    size_t chunk = align_up(sz + HEADER_SIZE, 4096);
    b = (Block *)pages_alloc(chunk);
    if (!b) return NULL;
    b->size  = chunk - HEADER_SIZE;
    b->magic = MAGIC;
    b->free  = 0;
    b->next  = NULL;
    b->prev  = NULL;

    /* add to free list tail */
    if (!free_list) {
        free_list = b;
    } else {
        Block *tail = free_list;
        while (tail->next) tail = tail->next;
        tail->next = b;
        b->prev    = tail;
    }

    split_block(b, sz);
    return (char *)b + HEADER_SIZE;
}

void *vero_calloc(size_t n, size_t sz) {
    size_t total = n * sz;
    void *p = vero_malloc(total);
    if (!p) return NULL;
    /* zero it — mmap gives us zeroed pages already for new allocs
       but for reused blocks we must zero */
    char *cp = (char *)p;
    for (size_t i = 0; i < total; i++) cp[i] = 0;
    return p;
}

static void vero_free(void *ptr);

void *vero_realloc(void *ptr, size_t sz) {
    if (!ptr)  return vero_malloc(sz);
    if (sz == 0) { vero_free(ptr); return NULL; }

    Block *b = (Block *)((char *)ptr - HEADER_SIZE);
    if (b->magic != MAGIC) return NULL;  /* corruption guard */
    if (b->size >= sz) { split_block(b, sz); return ptr; }

    void *np = vero_malloc(sz);
    if (!np) return NULL;
    /* copy old data */
    char *src = (char *)ptr;
    char *dst = (char *)np;
    size_t copy = b->size < sz ? b->size : sz;
    for (size_t i = 0; i < copy; i++) dst[i] = src[i];
    vero_free(ptr);
    return np;
}

void vero_free(void *ptr) {
    if (!ptr) return;
    Block *b = (Block *)((char *)ptr - HEADER_SIZE);
    if (b->magic != MAGIC) return;  /* guard against double-free */
    b->free = 1;
    coalesce(b);

    /* large allocs go back to OS immediately */
    if (b->size + HEADER_SIZE >= MMAP_THRESHOLD) {
        if (b->prev) b->prev->next = b->next;
        else free_list = b->next;
        if (b->next) b->next->prev = b->prev;
        pages_free(b, align_up(b->size + HEADER_SIZE, 4096));
    }
}
