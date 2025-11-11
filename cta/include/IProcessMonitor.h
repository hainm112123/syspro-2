#include "utils.h"
#include <vector>

class IProcessMonitor {
  public:
    IProcessMonitor() {}
    ~IProcessMonitor() {}

    virtual void SetConfigs(const std::vector<ProcessConfig> &configs) = 0;
    virtual void clean() = 0;
    virtual void scan() = 0;
    virtual void sample() = 0;
    virtual void run() = 0;
};
