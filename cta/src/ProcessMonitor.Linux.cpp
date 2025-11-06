#ifndef _WIN32
#include "ProcessMonitor.Linux.h"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>

static double getCurrentTime() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec / 1e9;
}

static inline bool readfile(const char *path, char *buf, size_t buf_size, ssize_t &len) {
  int fd = open(path, O_RDONLY | O_CLOEXEC);
  if (fd == -1) return false;
  len = read(fd, buf, buf_size - 1);
  close(fd);
  if (len <= 0) return false;
  buf[len] = '\0';
  return true;
}

static inline bool getProcessName(char *pid, std::string &pname) {
  std::string path = "/proc/" + std::string(pid) + "/comm";
  char buf[128];
  ssize_t len;
  if (!readfile(path.c_str(), buf, sizeof(buf), len)) return 0;
  if (len > 0 && buf[len - 1] == '\n') buf[len - 1] = '\0';
  pname.assign(buf);
  return 1;
} 

static unsigned long long getTotalCpuTime() {
  int fd = open("/proc/stat", O_RDONLY | O_CLOEXEC);
  if (fd < 0) return 0;
  char buf[1024];
  ssize_t n = read(fd, buf, sizeof(buf) - 1);
  close(fd);
  if (n <= 0) return 0;
  buf[n] = '\0';

  const char *ptr = buf + 4;
  unsigned long long total = 0;
  while (*ptr) {
    while (*ptr == ' ') ptr ++;
    if (!isdigit(*ptr)) break;
    total += strtoull(ptr, (char **)&ptr, 10);
  }
  return total;
}

static double getCPU(ProcessMonitor::ProcessInfo &info, const unsigned long long &cpuTime) {
  char buf[4096];
  unsigned long long procTime = 0;
  for (const auto &pid: info.pids) {
    char statPath[27];
    snprintf(statPath, sizeof(statPath), "/proc/%d/stat", (uint32_t)pid);
    int statFd = open(statPath, O_RDONLY | O_CLOEXEC);
    if (statFd < 0) continue;
    ssize_t n = pread(statFd, buf, sizeof(buf) - 1, 0);
    if (n <= 0) {
      close(statFd);
      continue;
    }
    buf[n] = '\0';

    const char *ptr = buf;
    unsigned long long utime = 0, stime = 0;
    long rss = 0;
    for (int i = 0; i < 25; ++ i) {
      while (*ptr && *ptr == ' ') ptr ++;
      if (!(*ptr)) break;
      const char *start = ptr;
      while (*ptr && *ptr != ' ') ptr++;
      // std::cout << i << "\n";
      if (i == 13) utime = strtoull(start, (char **)&ptr, 10);
      else if (i == 14) stime = strtoull(start, (char **)&ptr, 10);
      else if (i == 23) rss = strtol(start, (char **)&ptr, 10);
      ptr ++;
    }

    procTime += utime + stime;
  }
  // std::cout << info.pid << " " << utime << " " << stime << " " << rss << "\n";
  double cpuUsage = 0.0;
  if (info.lastCpuTime > 0) {
    unsigned long long deltaProc = procTime - info.lastProcTime;
    unsigned long long deltaTotal = cpuTime - info.lastCpuTime;
    cpuUsage = 100.0 * (double)deltaProc / deltaTotal;
  }
  info.lastProcTime = procTime;
  info.lastCpuTime = cpuTime;

  // close(fd);
  return cpuUsage;
}

static size_t getMemory(ProcessMonitor::ProcessInfo &info) {
  char buf[1024];
  size_t memory = 0;
  for (const auto &pid: info.pids) {
    char statusPath[29];
    snprintf(statusPath, sizeof(statusPath), "/proc/%d/status", (uint32_t)pid);
    int statusFd = open(statusPath, O_RDONLY | O_CLOEXEC);
    if (statusFd < 0) continue;
    ssize_t n = pread(statusFd, buf, sizeof(buf) - 1, 0);
    size_t memKB = 0;
    if (n > 0) {
      buf[n] = '\0';
      const char *vmrss = strstr(buf, "VmRSS:");
      if (vmrss) memKB = strtoul(vmrss + 6, nullptr, 10);
    } else {
      close(statusFd);
    }
    memory += memKB;
  }
  return memory / 1024;
}

static double getDisk(ProcessMonitor::ProcessInfo &info) {
  char buf[1024];
  unsigned long long ioCount = 0;
  for (const auto &pid: info.pids) {
    char ioPath[25];
    snprintf(ioPath, sizeof(ioPath), "/proc/%d/io", (uint32_t)pid);
    int ioFd = open(ioPath, O_RDONLY | O_CLOEXEC);
    if (ioFd < 0) continue;
    ssize_t n = pread(ioFd, buf, sizeof(buf) - 1, 0);
    if (n <= 0) {
      close(ioFd);
      continue;
    }
    const char *ptr = buf;
    unsigned long long read_bytes = 0, write_bytes = 0;
    int found = 0;
    while (*ptr && found < 3) {
      if (strncmp(ptr, "read_bytes:", 11) == 0) {
        ptr += 11;
        while (*ptr == ' ' || *ptr == '\t') ptr ++;
        read_bytes = strtoull(ptr, (char **)&ptr, 10);
        found |= 1;
      } else if (strncmp(ptr, "write_bytes:", 12) == 0) {
        ptr += 12;
        while (*ptr == ' ' || *ptr == '\t') ptr ++;
        write_bytes = strtoull(ptr, (char **)&ptr, 10);
        found |= 2;
      }
      while (*ptr && *ptr != '\n') ptr ++;
      if (*ptr == '\n') ptr ++;
    }
    // std::cout << found << " " << read_bytes << " " << write_bytes << "\n";
    ioCount += read_bytes + write_bytes;
  }
  double currentTime = getCurrentTime();
  double diskUsage = (double)(ioCount - info.lastIO) / (1024.0 * 1024.0 * (currentTime - info.lastDiskTime));
  info.lastIO = ioCount;
  info.lastDiskTime = currentTime;
  return diskUsage;
}

static void bump_memlock_rlimit() {
  struct rlimit rlim_new = {RLIM_INFINITY, RLIM_INFINITY};
  if (setrlimit(RLIMIT_MEMLOCK, &rlim_new)) {
    perror("setrlimit(RLIMIT_MEMLOCK)");
  }
}

bool ProcessMonitor::initNetStat() {
  const char *obj_path = "monitor.bpf.o";
  bump_memlock_rlimit();

  obj = bpf_object__open_file(obj_path, nullptr);
  if (libbpf_get_error(obj)) {
    std::perror("Failed to open BPF object");
    return false;
  }
  int err = bpf_object__load(obj);
  if (err != 0) {
    std::perror("Failed to load BPF object");
    bpf_object__close(obj);
    return false;
  }
  struct bpf_program *prog;
  bpf_object__for_each_program(prog, obj) {
    const char *sec = bpf_program__section_name(prog);
    if (strncmp(sec, "kprobe/", 7) == 0) {
      struct bpf_link *link = bpf_program__attach_kprobe(prog, false, sec + 7);
      if (!link) {
        std::perror("Failed to attach kprobe");
      }
    }
  }

  int map_tcp_tx = bpf_object__find_map_fd_by_name(obj, "tcp_tx");
  int map_tcp_rx = bpf_object__find_map_fd_by_name(obj, "tcp_rx");
  int map_udp_tx = bpf_object__find_map_fd_by_name(obj, "udp_tx");
  int map_udp_rx = bpf_object__find_map_fd_by_name(obj, "udp_rx");
  netMaps.push_back(map_tcp_tx);
  netMaps.push_back(map_tcp_rx);
  netMaps.push_back(map_udp_tx);
  netMaps.push_back(map_udp_rx);

  if (map_tcp_tx < 0 || map_tcp_rx < 0 || map_udp_tx < 0 || map_udp_rx < 0) {
    std::perror("Failed to find maps");
    bpf_object__close(obj);
    return false;
  }

  return true;
}

ProcessMonitor::ProcessMonitor(const std::vector<ProcessConfig> &configs, int _monitorInterval): monitorInterval(_monitorInterval) {
  ticksPerSec = sysconf(_SC_CLK_TCK);
  pageSize = sysconf(_SC_PAGE_SIZE);
  SetConfigs(configs);
}

ProcessMonitor::~ProcessMonitor() {
  // bpf_object__close(obj);
  clean();
}

void ProcessMonitor::SetConfigs(const std::vector<ProcessConfig> &configs) {
  clean();
  for (auto &config: configs) processes.insert(std::make_pair(config.name, ProcessInfo(ProcessStat(config.cpu, config.memory, config.disk, config.network))));
}

void ProcessMonitor::clean() {
  for (auto &[name, info]: processes) {
    
  }
  processes.clear();
}

void ProcessMonitor::scan() {
  for (auto &[name, info]: processes) info.pids.clear();
  DIR *dir = opendir("/proc");
  if (!dir) return;
  struct dirent *entry;
  while ((entry = readdir(dir)) != nullptr) {
    if (entry->d_type != DT_DIR) continue;
    pid_t pid = atoi(entry->d_name);
    if (pid < 0) continue;
    std::string pname;
    if (!getProcessName(entry->d_name, pname)) continue;
    if (processes.find(pname) == processes.end()) continue;

    auto &process = processes[pname];
    process.pids.push_back(pid);
    process.lastDiskTime = getCurrentTime();
    process.lastNetTime = getCurrentTime();
  }
  closedir(dir);
}

void ProcessMonitor::sample() {
  unsigned long long cpuTime = getTotalCpuTime();

  auto getNet = [&](ProcessInfo &info) {
    uint64_t totalBytes = 0, val = 0;
    for (int net_map: netMaps) {
      val = 0;
      for (auto pid: info.pids) {
        if (bpf_map_lookup_elem(net_map, &pid, &val) == 0) {
          totalBytes += val;
        }
      }
    }

    double currentNetTime = getCurrentTime();
    double net = (double)(totalBytes - info.lastTotalBytes) / (1024.0 * (currentNetTime - info.lastNetTime));
    info.lastNetTime = currentNetTime;
    info.lastTotalBytes = totalBytes;
    return net;
  };

  for (auto &[name, info]: processes) {
    double cpu = getCPU(info, cpuTime);
    double memory = getMemory(info);
    double disk = getDisk(info);
    double net = getNet(info);
    if (cpu > info.limit.cpu || memory > info.limit.memory || disk > info.limit.disk || net > info.limit.network) {
      std::cout << std::fixed << std::setprecision(2) << "Alert: " << name << " " << info.pids.size() << " use: " << cpu << " " << memory << " " << disk << " " << net << "\n";
    }
  }
}

void ProcessMonitor::run() {
  // if (!initNetStat()) {
  //   return;
  // }
  while (1) {
    std::this_thread::sleep_for(std::chrono::seconds(monitorInterval));
    scan();
    sample();
  }
}

#endif
