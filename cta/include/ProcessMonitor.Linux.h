#pragma once
#ifndef PROCESS_MONITOR_LINUX_H
#define PROCESS_MONITOR_LINUX_H

#include "utils.h"
#include <unordered_map>
#include <map>
#include <vector>
#include <string>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>

class ProcessMonitor {
  public:
    struct ProcessInfo {
      pid_t pid = -1;
      int statFd = -1, statusFd = -1, ioFd = -1;
      unsigned long long lastUTime = 0, lastSTime = 0, lastTotalTime = 0, lastIO = 0;
      double lastDiskTime = 0, lastNetTime = 0;
      uint64_t lastTotalBytes = 0;
      ProcessStat limit;
      ProcessInfo() {}
      ProcessInfo(ProcessStat _limit): limit(_limit) {}
    };
  private: 
    std::unordered_map<std::string, ProcessInfo> processes;
    // int map_tcp_tx, map_tcp_rx, map_udp_tx, map_udp_rx;
    std::vector<int> netMaps;
    struct bpf_object *obj;
    int monitorInterval;
    long ticksPerSec, pageSize;
    bool initNetStat();
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
