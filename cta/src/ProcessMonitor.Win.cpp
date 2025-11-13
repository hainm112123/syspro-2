#ifdef _WIN32
#include "ProcessMonitor.Win.h"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>  
#define INITGUID

static const GUID GUID_DISK_IO = { 0x9d1d5d37, 0x5a7f, 0x4f3f, {0x89,0x65,0xe3,0x59,0x0d,0x6d,0x14,0xb0} };
static const GUID KERNEL_FILE_GUID = { 0xEDD08927, 0x9CC4, 0x4E65,{ 0xB3,0x3D,0xF1,0x2E,0x4F,0x4E,0xED,0xE0 } };
static const GUID DiskIoGuid = { 0x9e814aad, 0x3204, 0x11d2, {0x9a, 0x82, 0x00, 0x60, 0x08, 0xa8, 0x69, 0x39} };

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

//...................................ETW.....................................................................................
VOID WINAPI EventRecordCallback(PEVENT_RECORD pEventRecord) {
  std::wcout << L"Some Event: ID="
    << pEventRecord->EventHeader.EventDescriptor.Id
    << L" PID=" << pEventRecord->EventHeader.ProcessId
    << L" ProviderID=" << pEventRecord->EventHeader.ProcessId
    << L" Opcode=" << pEventRecord->EventHeader.EventDescriptor.Opcode
    << std::endl;
  
  if (pEventRecord->EventHeader.ProviderId == DiskIoGuid) {
    std::wcout << L"Disk I/O Event: ID="
      << pEventRecord->EventHeader.EventDescriptor.Id
      << L" PID=" << pEventRecord->EventHeader.ProcessId
      << std::endl;
  }
  if (pEventRecord->EventHeader.ProviderId != KERNEL_FILE_GUID)
    return;

  std::wcout << L"Kernel Event: ID="
    << pEventRecord->EventHeader.EventDescriptor.Id
    << L" PID=" << pEventRecord->EventHeader.ProcessId
    << std::endl;

  DWORD pid = pEventRecord->EventHeader.ProcessId;
  ULONG ioSize = 0;

  PROPERTY_DATA_DESCRIPTOR desc;
  ZeroMemory(&desc, sizeof(desc));
  desc.PropertyName = (ULONGLONG)L"Size";
  desc.ArrayIndex = ULONG_MAX;

  TDHSTATUS status = TdhGetProperty(pEventRecord, 0, nullptr, 1, &desc, sizeof(ioSize), (PBYTE)&ioSize);

  if (status != ERROR_SUCCESS) {
    return;
  }

  if (pEventRecord->EventHeader.EventDescriptor.Opcode == 10) {
    std::cout << "Read: " << ioSize << "\n";
  }
  else if (pEventRecord->EventHeader.EventDescriptor.Opcode == 11) {
    std::cout << "Write: " << ioSize << "\n";
  }
}

static void startETW() {
  TRACEHANDLE hSession = 0;

  ULONG bufferSize = sizeof(EVENT_TRACE_PROPERTIES) + 1024;
  EVENT_TRACE_PROPERTIES* pSessionProperties = (EVENT_TRACE_PROPERTIES*)malloc(bufferSize);
  ZeroMemory(pSessionProperties, bufferSize);
  pSessionProperties->Wnode.BufferSize = bufferSize;
  pSessionProperties->Wnode.ClientContext = 1;
  pSessionProperties->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
  wcscpy_s((wchar_t*)(pSessionProperties + 1), 1024, KERNEL_LOGGER_NAME);
  pSessionProperties->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;

  ControlTrace(0, KERNEL_LOGGER_NAME, pSessionProperties, EVENT_TRACE_CONTROL_STOP);
  ULONG status = StartTrace(&hSession, KERNEL_LOGGER_NAME, pSessionProperties);
  if (status != ERROR_SUCCESS) {
    std::cout << "StartTrace failed: " << status << "\n";
    return;
  }

  EVENT_TRACE_LOGFILE logFile = {};
  logFile.LoggerName = (LPWSTR)KERNEL_LOGGER_NAME;
  logFile.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD;
  logFile.EventRecordCallback = EventRecordCallback;

  TRACEHANDLE hTrace = OpenTrace(&logFile);
  if (hTrace == INVALID_PROCESSTRACE_HANDLE) {
    std::cout << "OpenTrace failed\n";
    return;
  }

  std::cout << "Start tracing disk io\n";
  ProcessTrace(&hTrace, 1, nullptr, nullptr);
}

//..................main....................................................................................................

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
    if (cpu > info.limit.cpu || memory > info.limit.memory || disk > info.limit.disk) {
      std::cout << wstringToString(name) << " " << info.pids.size() << " " << cpu << " " << memory << " " << disk << "\n";
    }
    if (cpu > info.limit.cpu) addEvent(OveruseEvent(info.pids[0], wstringToString(name), "cpu", cpu, get_timestamp()));
    if (memory > info.limit.memory) addEvent(OveruseEvent(info.pids[0], wstringToString(name), "memory", memory, get_timestamp()));
    if (disk > info.limit.disk) addEvent(OveruseEvent(info.pids[0], wstringToString(name), "disk", disk, get_timestamp()));
  }
}

void ProcessMonitor::run() {
  startETW();
  return;

  HANDLE hMap = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(SharedRing), FILE_MAP_NAME.c_str());
  if (!hMap) {
    printf("Create file mapping failed\n");
    return;
  }
  ring = (SharedRing*)MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
  if (!ring) {
    printf("Map view of file failed\n");
    CloseHandle(hMap);
    return;
  }
  hEvent = CreateEvent(nullptr, FALSE, FALSE, WIN_EVENT_NAME.c_str());
  if (!hEvent) {
    printf("Create event failed\n");
    UnmapViewOfFile(ring);
    CloseHandle(hMap);
    return;
  }
  ring->head = 0;
  ring->tail = 0;

  printf("Start monitoring....\n");
  printf("Press q to quit.\n");
  while (1) {
    if (_kbhit()) {
      char c = _getch();
      if (c == 'q') break;
    }
    scan();
    sample();
    std::this_thread::sleep_for(std::chrono::seconds(monitorInterval));
  }

  CloseHandle(hEvent);
  UnmapViewOfFile(ring);
  CloseHandle(hMap);
}

void ProcessMonitor::addEvent(const OveruseEvent& event) {
  //printf("{\"pid\":%d,\"name\":\"%s\",\"type\":%s,\"value\":%.2f,\"time\":%s}\n", event.pid, event.name, event.type, event.value, event.time);
  LONG head = ring->head;
  LONG tail = ring->tail;
  LONG next = (tail + 1) & (RING_SIZE - 1);
  if (next == head) {
    return;
  }
  ring->buffer[tail] = event;
  InterlockedExchange(&ring->tail, next);
  SetEvent(hEvent);
}

#endif
