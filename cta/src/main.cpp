#include "ProcessMonitor.Linux.h"

int main() {
    std::vector<ProcessConfig> configs;
    configs.push_back(ProcessConfig("node", 20, 20, 20, 20));
    configs.push_back(ProcessConfig("cpptools", 20, 20, 20, 20));
    ProcessMonitor monitor(configs);
    monitor.run();

    return 0;
}
