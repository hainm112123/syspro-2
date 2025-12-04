#ifdef _WIN32
#include "ProcessMonitor.Win.h"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>  

static std::wstring GetProcessNameByPid(DWORD pid) {
  std::wstring name = L"<unkown>";
  HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
  if (hProcess) {
    WCHAR buffer[MAX_WINS_PROC_PATH];
    DWORD size = MAX_WINS_PROC_PATH;
    if (QueryFullProcessImageName(hProcess, 0, buffer, &size)) {
      name = buffer;
    }
    CloseHandle(hProcess);
  }
  size_t pos = name.find_last_of(L"\\/");
  if (pos == std::wstring::npos) return name;
  return name.substr(pos + 1);
}

ProcessMonitor::ProcessMonitor(const std::vector<ProcessConfig> &configs, int _monitorInterval): monitorInterval(_monitorInterval) {
  HRESULT hResult = FilterConnectCommunicationPort(L"\\UsageDiskIoFilterPort", 0, NULL, 0, NULL, &hUsagePort);
  if (IS_ERROR(hResult)) {
    printf("Cannot connect to filter's usage port...\n");
    return;
  }
  printf("Connected to filter's usage port\n");
  procLock = SRWLOCK_INIT;
  SetConfigs(configs);
}

ProcessMonitor::~ProcessMonitor() {
  CloseHandle(hUsagePort);
  clean();
}

void ProcessMonitor::SetConfigs(const std::vector<ProcessConfig> &configs) {
  clean();
  for (auto& config : configs) {
    processes.insert(std::make_pair(stringToWstring(config.name), ProcessInfo(ProcessStat(config.cpu, config.memory, config.disk, config.network))));
  }
  for (auto& [name, info] : processes) info.lastTime = std::chrono::steady_clock::now();
  scan();
}

void ProcessMonitor::clean() {
  for (auto &[name, info]: processes) {
    
  }
  processes.clear();
}

static ULONGLONG fileTimeToULL(const FILETIME &ft) {
  return ((ULONGLONG)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
}

static void getStats(ProcessMonitor::ProcessInfo& info, ULONGLONG sysDelta, double &cpu, double &memory, double &disk, double &network) {
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

    /*IO_COUNTERS io;
    if (GetProcessIoCounters(hProcess, &io)) {
      totalIO += io.ReadTransferCount + io.WriteTransferCount;
    }*/

    CloseHandle(hProcess);
  }

  auto now = std::chrono::steady_clock::now();
  std::chrono::duration<double> elapsed = now - info.lastTime;
  cpu = (sysDelta > 0 && procTime > 0) ? 100.0 * (double)(procTime - info.lastProcTime) / sysDelta : 0.0;
  memory /= (1024.0 * 1024);
  disk = (double)(info.curIo) / (1024.0 * 1024 * elapsed.count());
  network = (double)(info.curNet) / (1024.0 * elapsed.count());
  // double curIo = info.curIo, curNet = info.curNet;
  // disk = (double)(curIo - info.lastIo) / (1024.0 * 1024 * elapsed.count());
  // network = (double)(curNet - info.lastNet) / (1024.0 * elapsed.count());
  // info.lastIo = curIo;
  // info.lastNet = curNet;
  info.lastProcTime = procTime;
}

//..................main....................................................................................................

void ProcessMonitor::sendTrackRequest(HANDLE hPort, TRACK_REQUEST request) {
  // printf("Send track request type: %u, process: %u\n", request.type, request.ProcessId);
  DWORD bytesReturned;
  FilterSendMessage(hPort, &request, sizeof(TRACK_REQUEST), NULL, 0, &bytesReturned);
}

void ProcessMonitor::getUsage() {
  sendTrackRequest(hUsagePort, {TYPE_REQUEST_STAT, 0});
  BYTE buffer[1024 * 64];
  auto message = (PFILTER_MESSAGE_HEADER)buffer;
  HRESULT hResult = FilterGetMessage(hUsagePort, message, sizeof(buffer), NULL);
  if (SUCCEEDED(hResult)) {
    auto usageList = (PPROCESS_USAGE_LIST)(buffer + sizeof(FILTER_MESSAGE_HEADER));
    for (int i = 0; i < usageList->Count; ++ i) {
      addIoCount(usageList->Entries[i].IoBytes, usageList->Entries[i].ProcessId);
      addNetCount(usageList->Entries[i].NetBytes, usageList->Entries[i].ProcessId);
    }
    printf("Succeeded get usage for %u processes\n", usageList->Count);
  } else {
    printf("Get usage failed...\n");
  }
}

void ProcessMonitor::scan() {
  AcquireSRWLockExclusive(&procLock);
  for (auto &[name, info]: processes) info.pids.clear();
  procInfo.clear();
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot == INVALID_HANDLE_VALUE) {
    ReleaseSRWLockExclusive(&procLock);
    return;
  }
  PROCESSENTRY32 entry{ sizeof(entry) };
  if (Process32First(snapshot, &entry)) {
    do {
      std::wstring pname = entry.szExeFile;
      if (processes.find(pname) != processes.end()) {
        processes[pname].pids.push_back(entry.th32ProcessID);
        procInfo[entry.th32ProcessID] = &processes[pname];
        sendTrackRequest(hUsagePort, {TYPE_TRACK_PROCESS, entry.th32ProcessID});
      }
    } while (Process32Next(snapshot, &entry));
  }
  ReleaseSRWLockExclusive(&procLock);
  CloseHandle(snapshot);
}

void ProcessMonitor::handleProcessMessage(HANDLE hPort, const BYTE* buffer) {
  auto msg = (PPROCESS_MESSAGE)(buffer);
  msg->IsCreate = (bool)(int)(msg->IsCreate);
  auto pname = GetProcessNameByPid(msg->ProcessId);
  if (processes.count(pname) > 0) {
    AcquireSRWLockExclusive(&procLock);
    auto &pids = processes[pname].pids;
    if (msg->IsCreate) {
      pids.push_back(msg->ProcessId);
      procInfo[msg->ProcessId] = &processes[pname];
      sendTrackRequest(hPort, {TYPE_TRACK_PROCESS, msg->ProcessId});
    } else {
      auto it = std::remove_if(pids.begin(), pids.end(), [msg](const auto &pid){
        return pid == msg->ProcessId;
      });
      pids.erase(it, pids.end());
      auto it2 = procInfo.find(msg->ProcessId);
      if (it2 != procInfo.end()) procInfo.erase(it2);
      sendTrackRequest(hPort, {TYPE_UNTRACK_PROCESS, msg->ProcessId});
    }
    ReleaseSRWLockExclusive(&procLock);
  }
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

  // printf("Tried to acquire lock to monitoring...\n");
  AcquireSRWLockShared(&procLock);
  // printf("Acquried lock to monitoring.\n");
  for (auto &[name, info]: processes) {
    info.curIo = 0;
    info.curNet = 0;
  }
  getUsage();
  for (auto &[name, info]: processes) {
    double cpu = 0, memory = 0, disk = 0, network = 0;
    getStats(info, sysDelta, cpu, memory, disk, network);
    if (cpu > info.limit.cpu || memory > info.limit.memory || disk > info.limit.disk) {
      std::cout << wstringToString(name) << " " << info.pids.size() << " " << cpu << " " << memory << " " << disk << " " << network << "\n";
    }
    if (cpu > info.limit.cpu) addEvent(OveruseEvent(info.pids[0], wstringToString(name), "cpu", cpu, get_timestamp()));
    if (memory > info.limit.memory) addEvent(OveruseEvent(info.pids[0], wstringToString(name), "memory", memory, get_timestamp()));
    if (disk > info.limit.disk) addEvent(OveruseEvent(info.pids[0], wstringToString(name), "disk", disk, get_timestamp()));
    if (network > info.limit.network) addEvent(OveruseEvent(info.pids[0], wstringToString(name), "network", network, get_timestamp()));
  }
  ReleaseSRWLockShared(&procLock);
}

void ProcessMonitor::run() {
  std::cout << "Start tracing...\n";
  //...........................Tracing thread.....................................................................
  HANDLE hStopTracing = CreateEvent(NULL, TRUE, FALSE, NULL);
  std::thread traceThread([&]() {
    HANDLE hPort;
    HRESULT hResult = FilterConnectCommunicationPort(L"\\DiskIoFilterPort", 0, NULL, 0, NULL, &hPort);
    if (IS_ERROR(hResult)) {
      printf("Cannot connect to filter's port...\n");
      return;
    }
    while (1) {
      BYTE buffer[sizeof(FILTER_MESSAGE_HEADER) + sizeof(PROCESS_MESSAGE)];
      auto message = (FILTER_MESSAGE_HEADER*)buffer;
      OVERLAPPED ov = {0};
      RtlZeroMemory(&ov, sizeof(ov));
      ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
      hResult = FilterGetMessage(hPort, message, sizeof(buffer), &ov);
      if (hResult == HRESULT_FROM_WIN32(ERROR_IO_PENDING)) {
        HANDLE waiters[] = {hStopTracing, ov.hEvent};
        DWORD idx = WaitForMultipleObjects(2, waiters, FALSE, INFINITE);
        // printf("Event notified: %u\n", idx);
        if (idx - WAIT_OBJECT_0 == 0) {
          CancelIoEx(hPort, &ov);
          WaitForSingleObject(ov.hEvent, 1000);
          printf("Exit tracing loop...\n");
          break;
        }
        DWORD bytes;
        if (GetOverlappedResult(hPort, &ov, &bytes, FALSE)) {
          handleProcessMessage(hPort, buffer + sizeof(FILTER_MESSAGE_HEADER));
        } else {
          printf("GetOverlappedResult failed: %u\n", GetLastError());
          break;
        }
      } else if (SUCCEEDED(hResult)) {
        handleProcessMessage(hPort, buffer + sizeof(FILTER_MESSAGE_HEADER));
      }
      else {
        std::cout << "Get message failed with code: " << HRESULT_CODE(hResult) << "\n";
        break;
      }
    }
    CloseHandle(hPort);
    printf("Tracing thread exited\n");
  });

  //...................................MMAP......................................
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

  //...................monitoring...................................................
  printf("Start monitoring....\n");
  printf("Press q to quit.\n");
  while (1) {
    if (_kbhit()) {
      char c = _getch();
      printf("Keyboard hit: %c\n", c);
      if (c == 'q') break;
    }
    // scan();
    sample();
    std::this_thread::sleep_for(std::chrono::seconds(monitorInterval));
  }

  //.....................................................................
  CloseHandle(hEvent);
  UnmapViewOfFile(ring);
  CloseHandle(hMap);

  //.....................................................................
  if (!SetEvent(hStopTracing)) {
    printf("Failed to set stop tracing event...\n");
  } else {
    printf("Stop tracing event set..\n");
    traceThread.join();
    printf("Tracing thread joined\n");
  }
  CloseHandle(hStopTracing);
}

void ProcessMonitor::addEvent(const OveruseEvent& event) {
  //printf("{\"pid\":%d,\"name\":\"%s\",\"type\":%s,\"value\":%.2f,\"time\":%s}\n", event.pid, event.name, event.type, event.value, event.time);
  LONG head = ring->head;
  LONG tail = ring->tail;
  LONG next = (tail + 1) & (RING_SIZE - 1);
  if (next == head) {
    if ((int)events.size() < MAX_QUEUE_SIZE) events.push(event);
    return;
  }
  if (!events.empty()) {
    events.push(event);
    while (next != head && !events.empty()) {
      ring->buffer[tail] = events.front();
      events.pop();
      tail = next;
      next = (tail + 1) & (RING_SIZE - 1);
    }
    InterlockedExchange(&ring->tail, tail);
    SetEvent(hEvent);
  }
  else {
    ring->buffer[tail] = event;
    InterlockedExchange(&ring->tail, next);
    SetEvent(hEvent);
  }
}

#endif
