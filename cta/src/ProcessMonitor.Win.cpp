#ifdef _WIN32
#include "ProcessMonitor.Win.h"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>  
#define INITGUID

const static GUID DiskIO_Provider_GUID = { 0x9c1f9f5e, 0x5645, 0x474f, { 0x81, 0x7f, 0x11, 0x93, 0x4a, 0x3c, 0xa0, 0xe0 } };

ProcessMonitor::ProcessMonitor(const std::vector<ProcessConfig> &configs, int _monitorInterval): monitorInterval(_monitorInterval) {
  SetConfigs(configs);
}

ProcessMonitor::~ProcessMonitor() {
  clean();
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

void WINAPI ProcessEvent(PEVENT_TRACE pEvent);
static void startEtwMonitoring() {
  ULONG status = ERROR_SUCCESS;
  EVENT_TRACE_LOGFILE trace;
  TRACE_LOGFILE_HEADER* pHeader = &trace.LogfileHeader;
  TRACEHANDLE hTrace = 0;

  ZeroMemory(&trace, sizeof(EVENT_TRACE_LOGFILE));
  trace.LogFileName = (LPWSTR)L"test.etl";
  trace.EventCallback = (PEVENT_CALLBACK)(ProcessEvent);

  hTrace = OpenTrace(&trace);
  if ((TRACEHANDLE)INVALID_HANDLE_VALUE == hTrace) {
    wprintf(L"OpenTrace failed with %lu\n", GetLastError());
    goto cleanup;
  }

  status = ProcessTrace(&hTrace, 1, 0, 0);
  if (status != ERROR_SUCCESS && status != ERROR_CANCELLED) {
    wprintf(L"ProcessTrace failed with %lu\n", status);
    goto cleanup;
  }

cleanup:
  if ((TRACEHANDLE)INVALID_HANDLE_VALUE != hTrace) {
    status = CloseTrace(hTrace);
  }
}
void WINAPI ProcessEvent(PEVENT_TRACE pEvent) {
  if (IsEqualGUID(pEvent->Header.Guid, EventTraceGuid) && pEvent->Header.Class.Type == EVENT_TRACE_TYPE_INFO) {
    ; // Skip this event.
  }
  else {
    
  }
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
  //startEtwMonitoring();
  while (1) {
    std::this_thread::sleep_for(std::chrono::seconds(monitorInterval));
    scan();
    sample();
  }
}

#endif
