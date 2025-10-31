#pragma once
#ifndef PROCESS_MONITOR_LINUX_H
#define PROCESS_MONITOR_LINUX_H

#include "utils.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

class ProcessMonitor {
  public:
    struct ProcessInfo {
      pid_t pid = -1;
      int statFd = -1, statusFd = -1;
      unsigned long long lastUTime = 0, lastSTime = 0, lastTotalTime = 0;
      ProcessStat limit;
      ProcessInfo() {}
      ProcessInfo(ProcessStat _limit): limit(_limit) {}
    };
  private: 
    std::unordered_map<std::string, ProcessInfo> processes;
    int monitorInterval;
    long ticksPerSec, pageSize;
  public:
    ProcessMonitor(const std::vector<ProcessConfig> &configs, int _monitorInterval = 1);
    ~ProcessMonitor();
    void SetConfigs(const std::vector<ProcessConfig> &configs);
    void clean();
    void scan();
    void sample();
    void run();
};

#endif
