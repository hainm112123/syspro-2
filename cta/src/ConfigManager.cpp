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
  configs = {{"node", 10, 10, 10}, {"cpptools-srv", 20, 20, 20, 20}};
  nlohmann::json jsonConf = configs;
  std::string str_conf = jsonConf.dump();
  int fd = open(linuxConfigPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) return;
  write(fd, str_conf.data(), str_conf.size());
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
