#pragma once
#ifndef PROCESS_MONITOR_LINUX_H
#define PROCESS_MONITOR_LINUX_H

#include "utils.h"
#include "IProcessMonitor.h"
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
#include <sys/socket.h>
#include <sys/eventfd.h>
#include <sys/un.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include <sys/mman.h>
#include <queue>

class ProcessMonitor : public IProcessMonitor {
  public:
    struct ProcessInfo {
      std::vector<pid_t> pids;
      unsigned long long lastProcTime = 0, lastCpuTime = 0, lastIO = 0;
      double lastDiskTime = 0, lastNetTime = 0;
      uint64_t lastTotalBytes = 0;
      ProcessStat limit;
      ProcessInfo() {}
      ProcessInfo(ProcessStat _limit): limit(_limit) {}
    };
  private: 
    const std::string UNIX_SOCKET_PATH = "/tmp/eventfd.sock";
    const std::string UNIX_EVENT_PATH = "/overuse_events";

    int eventFd, unixSocketFd;
    std::unordered_map<std::string, ProcessInfo> processes;
    // int map_tcp_tx, map_tcp_rx, map_udp_tx, map_udp_rx;
    std::vector<int> netMaps;
    struct bpf_object *obj;
    int monitorInterval;
    long ticksPerSec, pageSize;
    SharedRing *ring;
    std::queue<OveruseEvent> events;
    int mmapFd = -1;

    bool initNetStat();
  public:
    ProcessMonitor(const std::vector<ProcessConfig> &configs, int _monitorInterval = 1);
    ~ProcessMonitor();
    void SetConfigs(const std::vector<ProcessConfig> &configs);
    void clean();
    void scan();
    void sample();
    void run();
    void* sendEventFd();
    void addEvent(const pid_t &pid, const std::string &name, const std::string &type, const double &value, const std::string &timestamp);
};

#endif
