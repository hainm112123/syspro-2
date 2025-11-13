#ifdef _WIN32
#include "ProcessMonitor.Win.h"
#else
#include "ProcessMonitor.Linux.h"
#endif
#include "ConfigManager.h"
#include <iostream>

int main() {
    ConfigManager cnfMgr;
    //cnfMgr.saveConfig();
    auto configs = cnfMgr.getConfig();
    for (auto &conf: configs) std::cout << conf.name << " " << conf.cpu << "\n";
    ProcessMonitor monitor(configs, 1);
    monitor.run();

    return 0;
}
