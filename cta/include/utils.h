#pragma once

#include <string>
#include <time.h>

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
