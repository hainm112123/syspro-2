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

void ProcessMonitor::SetConfigs(const std::vector<ProcessConfig> &configs) {
  clean();
  for (auto &config: configs) processes.insert(std::make_pair(config.name, ProcessInfo(ProcessStat(config.cpu, config.memory, config.disk, config.network))));
}

void ProcessMonitor::clean() {
  for (auto &[name, info]: processes) {
    
  }
  processes.clear();
}

static ULONGLONG fileTimeToULL(const FILETIME &ft) {
  return ((ULONGLONG)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
}

static std::wstring stringToWstring(const std::string &str) {
  std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
  return conv.from_bytes(str);
}

static std::vector<DWORD> findProcessesByName(const std::wstring processName) {
  std::vector<DWORD> pids;
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot == INVALID_HANDLE_VALUE) return pids;
  PROCESSENTRY32 entry{ sizeof(entry) };
  if (Process32First(snapshot, &entry)) {
    do {
      if (_wcsicmp(entry.szExeFile, processName.c_str()) == 0)
        pids.push_back(entry.th32ProcessID);
    } while (Process32Next(snapshot, &entry));
  }
  CloseHandle(snapshot);
  return pids;
}

static ULONGLONG getProcessTimesData(DWORD pid) {
  HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
  if (!hProcess) return false;

  FILETIME create, exit, kernel, user;
  if (!GetProcessTimes(hProcess, &create, &exit, &kernel, &user)) {
    CloseHandle(hProcess);
    return 0;
  }

  ULONGLONG procTime = fileTimeToULL(kernel) + fileTimeToULL(user);
  CloseHandle(hProcess);
  return procTime;
}

static double getCPU(ProcessMonitor::ProcessInfo &info, ULONGLONG sysDelta) {
  ULONGLONG procTime = 0;
  for (auto &pid: info.pids) {
    procTime += getProcessTimesData(pid);
  }
  if (procTime == 0) {
    info.lastProcTime = procTime;
    return 0;
  }
  //std::cout << info.lastProcTime << " " << procTime << " " << sysDelta << "\n";
  double cpu = sysDelta > 0 ? 100.0 * (double)(procTime - info.lastProcTime) / sysDelta : 0.0;
  info.lastProcTime = procTime;
  return cpu;
}

void ProcessMonitor::scan() {
  for (auto &[name, info]: processes) info.pids = findProcessesByName(stringToWstring(name)); 
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
    double cpu = getCPU(info, sysDelta);
    std::cout << name << " " << info.pids.size() << " " << cpu << "\n";
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
