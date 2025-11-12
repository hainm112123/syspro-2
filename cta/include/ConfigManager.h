#include "utils.h"
#ifdef _WIN32
#define _UNICODE
#define UNICODE
#include <Windows.h>
#include <tchar.h>
#include <winsock2.h>
#include <WS2tcpip.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#endif
#include <vector>
#include <nlohmann/json.hpp>
#include <string>

class ConfigManager {
  const std::string linuxConfigPath = "/etc/syspro-2/cta/config.json";
  const uint32_t port = 8000;
  const std::string host = "127.0.0.1";
  std::vector<ProcessConfig> configs;
public:
  ConfigManager() {
    #ifndef _WIN32
      mkdir("/etc/syspro-2", 0755);
      mkdir("/etc/syspro-2/cta", 0755);
    #endif
  }
  ~ConfigManager() {}

  bool getNewConfig();
  void loadConfig();
  void saveConfig();
  std::vector<ProcessConfig> getConfig();
};
