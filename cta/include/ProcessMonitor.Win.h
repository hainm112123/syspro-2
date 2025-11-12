#pragma once
#ifndef PROCESS_MONITOR_WIN_H
#define PROCESS_MONITOR_WIN_H

#define UNICODE
#define _UNICODE
#include "utils.h"
#include "IProcessMonitor.h"
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
#include <evntrace.h>
#include <evntprov.h>
#include <wbemidl.h>
#include <wmistr.h>
#include <conio.h>
#include <tchar.h>
#pragma comment(lib, "advapi32.lib")

class ProcessMonitor: public IProcessMonitor {
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
  const std::wstring FILE_MAP_NAME = _TEXT("SharedOveruseEventRing");
  const std::wstring WIN_EVENT_NAME = _TEXT("SharedRingEvent");
  std::unordered_map<std::wstring, ProcessInfo> processes;
  int monitorInterval;
  ULONGLONG lastSysTime = 0;
  SharedRing* ring = nullptr;
  HANDLE hEvent;

public:
  ProcessMonitor(const std::vector<ProcessConfig>& configs, int _monitorInterval = 1);
  ~ProcessMonitor();
  void SetConfigs(const std::vector<ProcessConfig>& configs);
  void clean();
  void scan();
  void sample();
  void run();
  void addEvent(const OveruseEvent &event);
};


#endif