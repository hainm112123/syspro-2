#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h> 
#include <bpf/bpf_core_read.h>

char LICENSE[] SEC("license") = "Dual BSD/GPL";

struct {
  __uint(type, BPF_MAP_TYPE_HASH);
  __uint(max_entries, 16384);
  __type(key, u32);
  __type(value, u64);
} tx_bytes SEC(".maps");

struct {
  __uint(type, BPF_MAP_TYPE_HASH);
  __uint(max_entries, 16384);
  __type(key, u32);
  __type(value, u64);
} rx_bytes SEC(".maps");

// struct {
//   __uint(type, BPF_MAP_TYPE_HASH);
//   __uint(max_entries, 16384);
//   __type(key, u32);
//   __type(value, u64);
// } udp_tx SEC(".maps");

// struct {
//   __uint(type, BPF_MAP_TYPE_HASH);
//   __uint(max_entries, 16384);
//   __type(key, u32);
//   __type(value, u64);
// } udp_rx SEC(".maps");

// SEC("fexit/tcp_sendmsg")
// int BPF_PROG(tcp_send_exit, struct socket *sock, struct msghdr *msg, size_t size, int ret) {
//   if (ret <= 0) return 0;
//   u32 pid = bpf_get_current_pid_tgid() >> 32;
//   u64 *total = bpf_map_lookup_elem(&tx_bytes, &pid);
//   if (total) __sync_fetch_and_add(total, ret);
//   else {
//       u64 init = ret;
//       bpf_map_update_elem(&tx_bytes, &pid, &init, BPF_ANY);
//   }
//   return 0;
// }

// SEC("fentry/tcp_cleanup_rbuf")
// int BPF_PROG(tcp_cleanup_entry, struct socket *sock, int copied) {
//   if (copied <= 0) return 0;
//   u32 pid = bpf_get_current_pid_tgid() >> 32;
//   u64 *total = bpf_map_lookup_elem(&rx_bytes, &pid);
//   if (total) __sync_fetch_and_add(total, copied);
//   else {
//       u64 init = copied;
//       bpf_map_update_elem(&rx_bytes, &pid, &init, BPF_ANY);
//   }
//   return 0;
// }

SEC("kretprobe/tcp_sendmsg")
int kret_tcp_sendmsg(struct pt_regs *ctx) {
  int ret = PT_REGS_RC(ctx);   
  if (ret <= 0) return 0;

  u32 pid = bpf_get_current_pid_tgid() >> 32;
  u64 *acc = bpf_map_lookup_elem(&tx_bytes, &pid);
  u64 val = (u32)ret;
  if (acc)
    __sync_fetch_and_add(acc, val);
  else
    bpf_map_update_elem(&tx_bytes, &pid, &val, BPF_ANY);

  return 0;
}

SEC("kretprobe/udp_sendmsg")
int kret_udp_sendmsg(struct pt_regs *ctx) {
  int ret = PT_REGS_RC(ctx);
  if (ret <= 0) return 0;

  u32 pid = bpf_get_current_pid_tgid() >> 32;
  u64 *acc = bpf_map_lookup_elem(&tx_bytes, &pid);
  u64 val = (u32)ret;
  if (acc)
    __sync_fetch_and_add(acc, val);
  else
    bpf_map_update_elem(&tx_bytes, &pid, &val, BPF_ANY);

  return 0;
}

SEC("kretprobe/tcp_recvmsg")
int kret_tcp_recvmsg(struct pt_regs *ctx) {
  int ret = PT_REGS_RC(ctx);    
  if (ret <= 0) return 0;

  u32 pid = bpf_get_current_pid_tgid() >> 32;
  u64 *acc = bpf_map_lookup_elem(&rx_bytes, &pid);
  u64 val = (u32)ret;
  if (acc)
    __sync_fetch_and_add(acc, val);
  else
    bpf_map_update_elem(&rx_bytes, &pid, &val, BPF_ANY);

  return 0;
}

SEC("kretprobe/udp_recvmsg")
int kret_udp_recvmsg(struct pt_regs *ctx) {
  int ret = PT_REGS_RC(ctx);
  if (ret <= 0) return 0;

  u32 pid = bpf_get_current_pid_tgid() >> 32;
  u64 *acc = bpf_map_lookup_elem(&rx_bytes, &pid);
  u64 val = (u32)ret;
  if (acc)
    __sync_fetch_and_add(acc, val);
  else
    bpf_map_update_elem(&rx_bytes, &pid, &val, BPF_ANY);

  return 0;
}