#pragma once
#ifndef PROCESS_MONITOR_WIN_H
#define PROCESS_MONITOR_WIN_H

#define UNICODE
#define _UNICODE
#include "utils.h"
#include <windows.h>
#include <psapi.h>
#include <iostream>
#include <unordered_map>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <tlhelp32.h>
#include <locale>
#include <codecvt>

class ProcessMonitor {
public:
  struct ProcessInfo {
    std::vector<DWORD> pids;
    ULONGLONG lastProcTime = 0, lastTotalIO = 0;
    std::chrono::steady_clock::time_point lastTime;
    ProcessStat limit;
    ProcessInfo() {}
    ProcessInfo(ProcessStat _limit) : limit(_limit) {}
  };
private:
  std::unordered_map<std::wstring, ProcessInfo> processes;
  int monitorInterval;
  long ticksPerSec, pageSize;
  ULONGLONG lastSysTime = 0;

public:
  ProcessMonitor(const std::vector<ProcessConfig>& configs, int _monitorInterval = 1);
  ~ProcessMonitor();
  void SetConfigs(const std::vector<ProcessConfig>& configs);
  void clean();
  void scan();
  void sample();
  void run();
};


#endif