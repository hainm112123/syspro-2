#pragma once

#include <string>
#include <time.h>
#include <chrono>
#ifdef _WIN32
#include <windows.h>
#endif

struct ProcessConfig {
  std::string name;
  double cpu, memory, disk, network;
  ProcessConfig(std::string _name = "", double _cpu = 0, double _memory = 0, double _disk = 0, double _network = 0) :
    name(_name), cpu(_cpu), memory(_memory), disk(_disk), network(_network) {
  }
};

struct ProcessStat {
  double cpu, memory, disk, network;
  ProcessStat(double _cpu = 0, double _memory = 0, double _disk = 0, double _network = 0) :
    cpu(_cpu), memory(_memory), disk(_disk), network(_network) {
  }
};

#ifdef _WIN32
std::wstring stringToWstring(const std::string& str);
std::string wstringToString(const std::wstring& wstr);

struct OveruseEvent {
  DWORD pid;
  std::wstring name;
  std::string type;
  double value;
  std::string time;
};
#else 
struct OveruseEvent {
  pid_t pid;
  std::string name;
  std::string type;
  double value;
  std::string time;
};
#endif
