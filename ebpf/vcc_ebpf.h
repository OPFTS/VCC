#ifndef VCC_EBPF_H
#define VCC_EBPF_H

/*
 * VeroCC eBPF helpers
 * Compile eBPF programs with:  vcc --ebpf -o prog.o prog.c
 * Load with libbpf or bpftool
 */

#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>

/* Common eBPF map definitions */
#define VCC_BPF_MAP_ARRAY(name, val_type, max_entries) \
    struct {                                            \
        __uint(type,  BPF_MAP_TYPE_ARRAY);              \
        __uint(max_entries, max_entries);               \
        __type(key,   __u32);                           \
        __type(value, val_type);                        \
    } name SEC(".maps")

#define VCC_BPF_MAP_HASH(name, key_type, val_type, max_entries) \
    struct {                                                     \
        __uint(type,  BPF_MAP_TYPE_HASH);                        \
        __uint(max_entries, max_entries);                        \
        __type(key,   key_type);                                 \
        __type(value, val_type);                                 \
    } name SEC(".maps")

#define VCC_BPF_PROG(section)  SEC(section)
#define VCC_BPF_LICENSE(l)     char _license[] SEC("license") = l

#endif /* VCC_EBPF_H */
