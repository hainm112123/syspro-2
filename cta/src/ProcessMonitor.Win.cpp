#ifdef _WIN32
#include "ProcessMonitor.Win.h"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>

//static double getCurrentTime() {
//  struct timespec ts;
//  clock_gettime(CLOCK_MONOTONIC, &ts);
//  return ts.tv_sec + ts.tv_nsec / 1e9;
//}

ProcessMonitor::ProcessMonitor(const std::vector<ProcessConfig> &configs, int _monitorInterval): monitorInterval(_monitorInterval) {
  //ticksPerSec = sysconf(_SC_CLK_TCK);
  //pageSize = sysconf(_SC_PAGE_SIZE);
  SetConfigs(configs);
}

ProcessMonitor::~ProcessMonitor() {
  clean();
}

static std::wstring stringToWstring(const std::string& str) {
  if (str.empty()) return L"";
  int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
  std::wstring wstr(size_needed, 0);
  MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], size_needed);
  wstr.pop_back(); 
  return wstr;
}

static std::string wstringToString(const std::wstring& wstr) {
  int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
  std::string str(size_needed, 0);
  WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], size_needed, NULL, NULL);
  str.pop_back();
  return str;
}

void ProcessMonitor::SetConfigs(const std::vector<ProcessConfig> &configs) {
  clean();
  for (auto& config : configs) {
    processes.insert(std::make_pair(stringToWstring(config.name), ProcessInfo(ProcessStat(config.cpu, config.memory, config.disk, config.network))));
  }
  for (auto& [name, info] : processes) info.lastTime = std::chrono::steady_clock::now();
}

void ProcessMonitor::clean() {
  for (auto &[name, info]: processes) {
    
  }
  processes.clear();
}

static ULONGLONG fileTimeToULL(const FILETIME &ft) {
  return ((ULONGLONG)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
}

static void getStats(ProcessMonitor::ProcessInfo& info, ULONGLONG sysDelta, double &cpu, double &memory, double &disk) {
  ULONGLONG procTime = 0;
  ULONGLONG totalIO = 0;
  for (auto& pid : info.pids) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!hProcess) continue;

    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(hProcess, (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
      memory += pmc.WorkingSetSize;
    }

    FILETIME create, exit, kernel, user;
    if (GetProcessTimes(hProcess, &create, &exit, &kernel, &user)) {
      procTime += fileTimeToULL(kernel) + fileTimeToULL(user);
    }

    IO_COUNTERS io;
    if (GetProcessIoCounters(hProcess, &io)) {
      totalIO += io.ReadTransferCount + io.WriteTransferCount;
    }

    CloseHandle(hProcess);
  }

  auto now = std::chrono::steady_clock::now();
  std::chrono::duration<double> elapsed = now - info.lastTime;
  cpu = (sysDelta > 0 && procTime > 0) ? 100.0 * (double)(procTime - info.lastProcTime) / sysDelta : 0.0;
  memory /= (1024.0 * 1024);
  disk = (double)(totalIO - info.lastTotalIO) / (1024.0 * 1024 * elapsed.count());
  info.lastProcTime = procTime;
}

void ProcessMonitor::scan() {
  for (auto &[name, info]: processes) info.pids.clear();
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot == INVALID_HANDLE_VALUE) return;
  PROCESSENTRY32 entry{ sizeof(entry) };
  if (Process32First(snapshot, &entry)) {
    do {
      std::wstring pname = entry.szExeFile;
      if (processes.find(pname) != processes.end()) processes[pname].pids.push_back(entry.th32ProcessID);
    } while (Process32Next(snapshot, &entry));
  }
  CloseHandle(snapshot);
}

void ProcessMonitor::sample() {
  FILETIME sysIdleNow, sysKernelNow, sysUserNow;
  GetSystemTimes(&sysIdleNow, &sysKernelNow, &sysUserNow);

  ULONGLONG sysKernelNowU = fileTimeToULL(sysKernelNow);
  ULONGLONG sysUserNowU = fileTimeToULL(sysUserNow);
  ULONGLONG sysDelta = 0;
  if (lastSysTime > 0) {
    sysDelta = sysKernelNowU + sysUserNowU - lastSysTime;
  }
  lastSysTime = sysKernelNowU + sysUserNowU;

  for (auto &[name, info]: processes) {
    double cpu = 0, memory = 0, disk = 0;
    getStats(info, sysDelta, cpu, memory, disk);
    std::cout << wstringToString(name) << " " << info.pids.size() << " " << cpu << " " << memory << " " << disk << "\n";
  }
}

void ProcessMonitor::run() {
  while (1) {
    std::this_thread::sleep_for(std::chrono::seconds(monitorInterval));
    scan();
    sample();
  }
}

#endif
