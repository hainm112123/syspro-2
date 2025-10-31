#ifndef _WIN32
#include "ProcessMonitor.Linux.h"
#include <iostream>
#include <thread>
#include <chrono>

static inline bool readfile(const char *path, char *buf, size_t buf_size, ssize_t &len) {
  int fd = open(path, O_RDONLY | O_CLOEXEC);
  if (fd == -1) return false;
  len = read(fd, buf, buf_size - 1);
  close(fd);
  if (len <= 0) return false;
  buf[len] = '\0';
  return true;
}

static inline bool getProcessName(pid_t pid, std::string &pname) {
  char path[64];
  snprintf(path, sizeof(path), "/proc/%d/comm", pid);
  char buf[128];
  ssize_t len;
  if (!readfile(path, buf, sizeof(buf), len)) return 0;
  if (len > 0 && buf[len - 1] == '\n') buf[len - 1] = '\0';
  pname.assign(buf);
  return 1;
} 

static inline double getMemory(pid_t pid) {
  char path[64];
  snprintf(path, sizeof(path), "/proc/%d/statm", pid);
  char buf[128];
  ssize_t len;
  if (!readfile(path, buf, sizeof(buf), len)) return 0;
  unsigned long pages = 0;
  sscanf(buf, "%lu", &pages);
  long page_size = sysconf(_SC_PAGE_SIZE);
  return (double)(pages * page_size) / (1024 * 1024);
  
}

ProcessMonitor::ProcessMonitor(const std::vector<ProcessConfig> &configs, int _monitorInterval): monitorInterval(_monitorInterval) {
  SetConfigs(configs);
}

void ProcessMonitor::SetConfigs(const std::vector<ProcessConfig> &configs) {
  processes.clear();
  for (auto &config: configs) processes[config.name] = {-1, -1, ProcessStat(config.cpu, config.memory, config.disk, config.network)};
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
    if (!getProcessName(pid, pname)) continue;
    if (processes.find(pname) == processes.end()) continue;
    processes[pname].pid = pid;
    processes[pname].stat.memory = getMemory(pid);
  }
  closedir(dir);
}

void ProcessMonitor::run() {
  while (1) {
    scan();
    for (auto &[name, info]: processes) {
      std::cout << name << " " << info.pid << " " << info.stat.memory << "\n";
    }
    std::this_thread::sleep_for(std::chrono::seconds(monitorInterval));
  }
}

#endif
