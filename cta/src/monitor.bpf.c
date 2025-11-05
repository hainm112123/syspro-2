// monitor.bpf.c
// Compile with:
// clang -O2 -g -target bpf -D__TARGET_ARCH_x86 -Iinclude -c src/monitor.bpf.c -o build/monitor.bpf.o

#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>   // <-- needed for BPF_KPROBE, BPF_KRETPROBE macros
#include <bpf/bpf_core_read.h>

char LICENSE[] SEC("license") = "Dual BSD/GPL";

/* Per-PID TX/RX maps */
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 16384);
    __type(key, u32);
    __type(value, u64);
} tcp_tx SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 16384);
    __type(key, u32);
    __type(value, u64);
} tcp_rx SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 16384);
    __type(key, u32);
    __type(value, u64);
} udp_tx SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 16384);
    __type(key, u32);
    __type(value, u64);
} udp_rx SEC(".maps");

/* ------------------------- TCP HOOKS ------------------------- */

/* tcp_sendmsg(struct sock *sk, struct msghdr *msg, size_t size) */
SEC("kprobe/tcp_sendmsg")
int BPF_KPROBE(kprobe__tcp_sendmsg, struct sock *sk, struct msghdr *msg, size_t size)
{
    if (size == 0)
        return 0;

    u32 pid = bpf_get_current_pid_tgid() >> 32;
    u64 *val = bpf_map_lookup_elem(&tcp_tx, &pid);
    if (val)
        __sync_fetch_and_add(val, (u64)size);
    else {
        u64 init = size;
        bpf_map_update_elem(&tcp_tx, &pid, &init, BPF_ANY);
    }
    return 0;
}

/* tcp_cleanup_rbuf(struct sock *sk, int copied) */
SEC("kprobe/tcp_cleanup_rbuf")
int BPF_KPROBE(kprobe__tcp_cleanup_rbuf, struct sock *sk, int copied)
{
    if (copied <= 0)
        return 0;

    u32 pid = bpf_get_current_pid_tgid() >> 32;
    u64 *val = bpf_map_lookup_elem(&tcp_rx, &pid);
    if (val)
        __sync_fetch_and_add(val, (u64)copied);
    else {
        u64 init = copied;
        bpf_map_update_elem(&tcp_rx, &pid, &init, BPF_ANY);
    }
    return 0;
}

/* ------------------------- UDP HOOKS ------------------------- */

/* udp_sendmsg(struct sock *sk, struct msghdr *msg, size_t len) */
SEC("kprobe/udp_sendmsg")
int BPF_KPROBE(kprobe__udp_sendmsg, struct sock *sk, struct msghdr *msg, size_t len)
{
    if (len == 0)
        return 0;

    u32 pid = bpf_get_current_pid_tgid() >> 32;
    u64 *val = bpf_map_lookup_elem(&udp_tx, &pid);
    if (val)
        __sync_fetch_and_add(val, (u64)len);
    else {
        u64 init = len;
        bpf_map_update_elem(&udp_tx, &pid, &init, BPF_ANY);
    }
    return 0;
}

/* udp_recvmsg(struct sock *sk, struct msghdr *msg, size_t len, int flags) */
SEC("kprobe/udp_recvmsg")
int BPF_KPROBE(kprobe__udp_recvmsg, struct sock *sk, struct msghdr *msg, size_t len, int flags)
{
    if (len <= 0)
        return 0;

    u32 pid = bpf_get_current_pid_tgid() >> 32;
    u64 *val = bpf_map_lookup_elem(&udp_rx, &pid);
    if (val)
        __sync_fetch_and_add(val, (u64)len);
    else {
        u64 init = len;
        bpf_map_update_elem(&udp_rx, &pid, &init, BPF_ANY);
    }
    return 0;
}
