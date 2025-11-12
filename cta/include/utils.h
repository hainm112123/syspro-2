#pragma once
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define _UNICODE
#define UNICODE
#include <Windows.h>
#endif
#include <string>
#include <time.h>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <stdint.h>

const int RING_SIZE = 1024;
const int EVENT_NAME_SIZE = 32;
const int EVENT_TYPE_SIZE = 16;
const int EVENT_TIME_SIZE = 32;

struct ProcessConfig {
  std::string name;
  double cpu, memory, disk, network;
  ProcessConfig(std::string _name = "", double _cpu = 0, double _memory = 0, double _disk = 0, double _network = 0): 
    name(_name), cpu(_cpu), memory(_memory), disk(_disk), network(_network) {}
};

struct ProcessStat {
  double cpu, memory, disk, network;
  ProcessStat(double _cpu = 0, double _memory = 0, double _disk = 0, double _network = 0): 
    cpu(_cpu), memory(_memory), disk(_disk), network(_network) {}
};

inline std::string get_timestamp() {
  using namespace std::chrono;
  auto now = system_clock::now();
  std::time_t t = system_clock::to_time_t(now);
  std::tm tm;
#ifdef _WIN32
  localtime_s(&tm, &t);
#else
  localtime_r(&t, &tm);
#endif

  std::ostringstream oss;
  oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
  return oss.str();
}


#ifdef _WIN32
std::wstring stringToWstring(const std::string& str);
std::string wstringToString(const std::wstring& wstr);
#endif

#ifdef _WIN32
struct OveruseEvent {
  DWORD pid;
  std::wstring name;
  std::string type;
  double value;
  std::string time;
};
#else 
struct OveruseEvent {
  uint32_t pid;
  char name[EVENT_NAME_SIZE];
  char type[EVENT_NAME_SIZE];
  double value;
  char time[EVENT_TIME_SIZE];
};

struct SharedRing {
  uint32_t head;
  uint32_t tail;
  OveruseEvent buffer[RING_SIZE];
} __attribute__((aligned(8)));
#endif
