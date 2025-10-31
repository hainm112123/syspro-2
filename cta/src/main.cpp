#include "ProcessMonitor.Linux.h"

int main() {
    std::vector<ProcessConfig> configs;
    // configs.push_back(ProcessConfig("node", 20, 20, 20, 20));
    // configs.push_back(ProcessConfig("cpptools", 20, 20, 20, 20));
    // configs.push_back(ProcessConfig("cpptools-srv", 20, 20, 20, 20));
    configs.push_back(ProcessConfig("cpu_busy", 0, 0, 0, 0));
    configs.push_back(ProcessConfig("ram_use", 0, 0, 0, 0));
    ProcessMonitor monitor(configs, 3);
    monitor.run();

    return 0;
}
