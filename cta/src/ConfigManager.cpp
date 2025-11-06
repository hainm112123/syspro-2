#include "ConfigManager.h"

bool ConfigManager::getNewConfig() {
  return false;
}

static void from_json(const nlohmann::json &j, ProcessConfig &config) {
  config.name = j.at("name").get<std::string>();
  config.cpu = j.at("cpu").get<double>();
  config.memory = j.at("memory").get<double>();
  config.disk = j.at("disk").get<double>();
  config.network = j.at("network").get<double>();
}

static void to_json(nlohmann::json &j, const ProcessConfig &config) {
  j = nlohmann::json{
    {"name", config.name},
    {"cpu", config.cpu},
    {"memory", config.memory},
    {"disk", config.disk},
    {"network", config.network}
  };
}

void ConfigManager::loadConfig() {
#ifdef _WIN32
  DWORD type = 0, size = 0;
  RegGetValue(HKEY_CURRENT_USER, _T("Software\\syspro-2\\cta"), _T("Config"), RRF_RT_REG_SZ, &type, nullptr, &size);
  std::wstring str(size / sizeof(wchar_t), _T('\0'));
  RegGetValue(HKEY_CURRENT_USER, _T("Software\\syspro-2\\cta"), _T("Config"), RRF_RT_REG_SZ, &type, str.data(), &size);
  auto jsonConf = nlohmann::json::parse(str.begin(), str.end());
  configs = jsonConf.get<std::vector<ProcessConfig>>();
#else
  int fd = open(linuxConfigPath.c_str(), O_RDONLY);
  if (fd < 0) return;
  struct stat st;
  fstat(fd, &st);
  std::vector<char> buffer(st.st_size);
  read(fd, buffer.data(), st.st_size);
  close(fd);
  auto jsonConf = nlohmann::json::parse(buffer.begin(), buffer.end());
  configs = jsonConf.get<std::vector<ProcessConfig>>();
#endif
}

void ConfigManager::saveConfig() {
  configs = { {"brave.exe", 10, 10, 10}, {"RTSS.exe", 20, 20, 20, 20} };
#ifdef _WIN32
  HKEY hKey;
  RegCreateKeyEx(HKEY_CURRENT_USER, _T("Software\\syspro-2\\cta"), 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL);
  nlohmann::json jsonConf = configs;
  std::string temp = jsonConf.dump();
  std::wstring str_conf = stringToWstring(temp);
  RegSetValueEx(hKey, _T("Config"), 0, REG_SZ, (BYTE*)str_conf.c_str(), (str_conf.size() + 1) * sizeof(wchar_t));
  RegCloseKey(hKey);
#else
  nlohmann::json jsonConf = configs;
  std::string str_conf = jsonConf.dump();
  int fd = open(linuxConfigPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) return;
  write(fd, str_conf.data(), str_conf.size());
#endif
  configs.clear();
}

std::vector<ProcessConfig> ConfigManager::getConfig() {
  if (!configs.empty()) return configs;
  if (getNewConfig()) {
    saveConfig();
  } else {
    loadConfig();
  }
  return configs;
}
