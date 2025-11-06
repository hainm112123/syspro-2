#include "utils.h"
#include <vector>
#include <nlohmann/json.hpp>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

class ConfigManager {
  const std::string linuxConfigPath = "/etc/syspro-2/cta/config.json";
  std::vector<ProcessConfig> configs;
public:
  ConfigManager() {}
  ~ConfigManager() {}

  bool getNewConfig();
  void loadConfig();
  void saveConfig();
  std::vector<ProcessConfig> getConfig();
};
