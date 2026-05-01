#include "../../ebpf/vcc_ebpf.h"

VCC_BPF_LICENSE("GPL");

VCC_BPF_MAP_ARRAY(pkt_count, __u64, 1);

VCC_BPF_PROG("xdp")
int vcc_xdp_prog(struct xdp_md *ctx) {
    __u32 key   = 0;
    __u64 *val  = bpf_map_lookup_elem(&pkt_count, &key);
    if (val) (*val)++;
    return XDP_PASS;
}
