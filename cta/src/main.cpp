#ifdef _WIN32
#include "ProcessMonitor.Win.h"
#else
#include "ProcessMonitor.Linux.h"
#endif

int main() {
    std::vector<ProcessConfig> configs;
    configs.push_back(ProcessConfig("stress_win.exe", 0, 0, 0, 0));
    configs.push_back(ProcessConfig("brave.exe", 0, 0, 0, 0));
    ProcessMonitor monitor(configs, 3);
    monitor.run();

    return 0;
}
