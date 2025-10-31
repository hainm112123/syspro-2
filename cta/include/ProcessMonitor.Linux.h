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
      pid_t pid;
      int fd;
      ProcessStat stat;
    };
  private: 
    std::unordered_map<std::string, ProcessInfo> processes;
    int monitorInterval;
  public:
    ProcessMonitor(const std::vector<ProcessConfig> &configs, int _monitorInterval = 1);
    void SetConfigs(const std::vector<ProcessConfig> &configs);
    void scan();
    void run();
};

#endif
