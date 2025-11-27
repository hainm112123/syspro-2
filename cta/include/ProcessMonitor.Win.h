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
#include <map>
#include <string>
#include <queue>
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
#include <tdh.h>
#include <evntcons.h>
#pragma comment(lib, "tdh.lib")
#pragma comment(lib, "advapi32.lib")

static const GUID GUID_DISK_IO = { 0x3d6fa8d4, 0xfe05, 0x11d0, { 0x9d, 0xda, 0x00, 0xc0, 0x4f, 0xd7, 0xba, 0x7c } };
static const GUID GUID_TCPIP = { 0x9a280ac0, 0xc8e0, 0x11d1, { 0x84, 0xe2, 0x00, 0xc0, 0x4f, 0xb9, 0x98, 0xa2 } };
static const GUID KERNEL_FILE_GUID = { 0xEDD08927, 0x9CC4, 0x4E65,{ 0xB3,0x3D,0xF1,0x2E,0x4F,0x4E,0xED,0xE0 } };

static const GUID DiskIoProviderGuid = { 0x9e814aad,0x3204,0x11d2,{0x9a,0x82,0x00,0x60,0x08,0xa8,0x69,0x39} };
static const GUID NetworkProviderGuid = { 0x7dd42a49,0x5329,0x4832,{0x8f,0x55,0x17,0x8c,0x65,0x4e,0x49,0x5f} };

class ProcessMonitor: public IProcessMonitor {
public:
  struct ProcessInfo {
    std::vector<DWORD> pids;
    ULONGLONG lastProcTime = 0, lastIo, lastNet = 0, curIo = 0, curNet = 0;
    std::chrono::steady_clock::time_point lastTime;
    ProcessStat limit;

    ProcessInfo() {}
    ProcessInfo(ProcessStat _limit) : limit(_limit) {}
  };
  typedef struct _IO_MESSAGE {
    HANDLE ProcessId;
    ULONG64 bytes;
  } IO_MESSAGE, * PIO_MESSAGE;
private:
  const std::wstring FILE_MAP_NAME = _TEXT("SharedOveruseEventRing");
  const std::wstring WIN_EVENT_NAME = _TEXT("SharedRingEvent");
  const int MAX_QUEUE_SIZE = 4096;
  std::unordered_map<std::wstring, ProcessInfo> processes;
  std::map<DWORD, ProcessInfo*> procInfo;
  int monitorInterval;
  ULONGLONG lastSysTime = 0;
  SharedRing* ring = nullptr;
  HANDLE hEvent;
  std::queue<OveruseEvent> events;

public:
  ProcessMonitor(const std::vector<ProcessConfig>& configs, int _monitorInterval = 1);
  ~ProcessMonitor();

  void addIoCount(ULONG dataSize, DWORD pid) {
    procInfo[pid]->curIo += dataSize;
  }
  void addNetCount(ULONG dataSize, DWORD pid) {
    procInfo[pid]->curNet += dataSize;
  }

  void SetConfigs(const std::vector<ProcessConfig>& configs);
  void clean();
  void scan();
  void sample();
  void run();
  void addEvent(const OveruseEvent &event);
  static VOID WINAPI EventRecordCallback(PEVENT_RECORD pEventRecord);
};


#endif