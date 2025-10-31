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

static unsigned long long getTotalCPUTime() {
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

static double getCPU(ProcessMonitor::ProcessInfo &info, const unsigned long long &totalTime) {
  char buf[4096];
  // int fd = open("/proc/58096/stat", O_RDONLY);
  ssize_t n = pread(info.statFd, buf, sizeof(buf) - 1, 0);
  if (n <= 0) {
    close(info.statFd);
    info.statFd = -1;
    return 0.0;
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

  // std::cout << info.pid << " " << utime << " " << stime << " " << rss << "\n";
  double cpuUsage = 0.0;
  if (info.lastTotalTime > 0) {
    unsigned long long deltaProc = (utime + stime) - (info.lastUTime + info.lastSTime);
    unsigned long long deltaTotal = totalTime - info.lastTotalTime;
    cpuUsage = 100.0 * (double)deltaProc / deltaTotal;
  }
  info.lastUTime = utime;
  info.lastSTime = stime;
  info.lastTotalTime = totalTime;

  // close(fd);
  return cpuUsage;
}

static size_t getMemory(ProcessMonitor::ProcessInfo &info) {
  char buf[1024];
  ssize_t n = pread(info.statusFd, buf, sizeof(buf) - 1, 0);
  size_t memKB = 0;
  if (n > 0) {
    buf[n] = '\0';
    const char *vmrss = strstr(buf, "VmRSS:");
    if (vmrss) memKB = strtoul(vmrss + 6, nullptr, 10);
  } else {
    close(info.statusFd);
    info.statusFd = -1;
  }
  return memKB / 1024;
}

static double getDisk(ProcessMonitor::ProcessInfo &info) {
  char buf[1024];
  ssize_t n = pread(info.ioFd, buf, sizeof(buf) - 1, 0);
  if (n <= 0) {
    close(info.ioFd);
    info.ioFd = -1;
    return 0.0;
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
  std::cout << found << " " << read_bytes << " " << write_bytes << "\n";
  double currentTime = getCurrentTime();
  double diskUsage = (double)(read_bytes + write_bytes - info.lastIO) / (1024.0 * 1024.0 * (currentTime - info.lastTime));
  info.lastIO = read_bytes + write_bytes;
  info.lastTime = currentTime;
  return diskUsage;
}

static double getNetwork(ProcessMonitor::ProcessInfo &info) {

}

ProcessMonitor::ProcessMonitor(const std::vector<ProcessConfig> &configs, int _monitorInterval): monitorInterval(_monitorInterval) {
  ticksPerSec = sysconf(_SC_CLK_TCK);
  pageSize = sysconf(_SC_PAGE_SIZE);
  SetConfigs(configs);
}

ProcessMonitor::~ProcessMonitor() {
  clean();
}

void ProcessMonitor::SetConfigs(const std::vector<ProcessConfig> &configs) {
  clean();
  for (auto &config: configs) processes.insert(std::make_pair(config.name, ProcessInfo(ProcessStat(config.cpu, config.memory, config.disk, config.network))));
}

void ProcessMonitor::clean() {
  for (auto &[name, info]: processes) {
    if (info.statFd >= 0) close(info.statFd);
    if (info.statusFd >= 0) close(info.statusFd);
    if (info.ioFd >= 0) close(info.ioFd);
  }
  processes.clear();
}

void ProcessMonitor::scan() {
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
    process.pid = pid;
    std::string statPath = "/proc/" + std::string(entry->d_name) + "/stat";
    std::string statusPath = "/proc/" + std::string(entry->d_name) + "/status";
    std::string ioPath = "/proc/" + std::string(entry->d_name) + "/io";
    process.statFd = open(statPath.c_str(), O_RDONLY | O_CLOEXEC);
    process.statusFd = open(statusPath.c_str(), O_RDONLY | O_CLOEXEC);
    process.ioFd = open(ioPath.c_str(), O_RDONLY | O_CLOEXEC);
    process.lastTime = getCurrentTime();
  }
  closedir(dir);
}

void ProcessMonitor::sample() {
  unsigned long long totalTime = getTotalCPUTime();
  for (auto &[name, info]: processes) {
    if (info.statFd < 0 || info.statusFd < 0 || info.ioFd < 0) continue;
    double cpu = getCPU(info, totalTime);
    double memory = getMemory(info);
    double disk = getDisk(info);
    if (cpu > info.limit.cpu || memory > info.limit.memory || disk > info.limit.disk) {
      std::cout << std::fixed << std::setprecision(2) << "Alert: " << name << " " << info.pid << " use: " << cpu << " " << memory << " " << disk << "\n";
    }
  }
}

void ProcessMonitor::run() {
  scan();
  while (1) {
    std::this_thread::sleep_for(std::chrono::seconds(monitorInterval));
    sample();
  }
}

#endif
