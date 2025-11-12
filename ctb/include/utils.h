#pragma once

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#include <windows.h>
#endif
#include <string>
#include <time.h>
#include <chrono>
#include <stdint.h>

const int RING_SIZE = 1024;
const int EVENT_NAME_SIZE = 32;
const int EVENT_TYPE_SIZE = 16;
const int EVENT_TIME_SIZE = 32;

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
#endif

struct OveruseEvent {
  uint32_t pid;
  char name[EVENT_NAME_SIZE];
  char type[EVENT_NAME_SIZE];
  double value;
  char time[EVENT_TIME_SIZE];
};

#ifdef _WIN32
struct SharedRing {
  volatile LONG head;
  volatile LONG tail;
  OveruseEvent buffer[RING_SIZE];
} __declspec(align(8));
#else 
struct SharedRing {
  uint32_t head;
  uint32_t tail;
  OveruseEvent buffer[RING_SIZE];
} __attribute__((aligned(8)));
#endif
