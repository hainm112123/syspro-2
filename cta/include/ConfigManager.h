#ifdef _WIN32
#define _UNICODE
#define UNICODE
#include <Windows.h>
#include <tchar.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include "utils.h"
#include <vector>
#include <nlohmann/json.hpp>
#include <string>

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
