#include "ConfigManager.h"
#include <iostream>

static void from_json(const nlohmann::json& j, ProcessConfig& config) {
  config.name = j.at("process").get<std::string>();
  config.cpu = j.at("cpu").get<double>();
  config.memory = j.at("memory").get<double>();
  config.disk = j.at("disk").get<double>();
  config.network = j.at("network").get<double>();
}

static void to_json(nlohmann::json& j, const ProcessConfig& config) {
  j = nlohmann::json{
    {"process", config.name},
    {"cpu", config.cpu},
    {"memory", config.memory},
    {"disk", config.disk},
    {"network", config.network}
  };
}

bool ConfigManager::getNewConfig() {
#ifdef _WIN32
  WSADATA wsaData;
  WSAStartup(MAKEWORD(2, 2), &wsaData);
  SOCKET client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#else
  int client = socket(AF_INET, SOCK_STREAM, 0);
#endif 
  
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
    perror("Invalid address\n");
    return false;
  }
  //printf("cp3\n");

#ifdef _WIN32
  if (connect(client, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
    printf("Connection failed: %d", WSAGetLastError());
    return false;
  }
#else
  if (connect(client, (sockaddr*)&addr, sizeof(addr)) < 0) return false;
#endif
  
  //printf("cp4\n");
  uint32_t len = 0;

  recv(client, (char*)&len, sizeof(len), MSG_WAITALL);
  //std::cout << len << "\n";
  len = ntohl(len);
  //std::cout << len << "\n";
  std::vector<char> buffer(len);
  recv(client, buffer.data(), len, MSG_WAITALL);

  try {
    //std::string str(buffer.begin(), buffer.end());
    auto json = nlohmann::json::parse(buffer.begin(), buffer.end());
    //for (auto item : json) {
    //  std::cout << item.dump() << "\n";
    //}
    configs = json.get<std::vector<ProcessConfig>>();
  }
  catch(std::exception e) {
    perror("json parse error");
    return false;
  }

#ifdef _WIN32
	closesocket(client);
	WSACleanup();
#else
	close(client);
#endif

  return true;
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
  // configs = { {"node", 0, 0, 10, 10}, {"cpptools-srv", 0, 0, 20, 20} };
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
  int fd = open(linuxConfigPath.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
  if (fd < 0) {
    perror("Invalid config fd");
    return;
  }
  write(fd, str_conf.data(), str_conf.size());
#endif
  // configs.clear();
}

std::vector<ProcessConfig> ConfigManager::getConfig() {
  if (!configs.empty()) return configs;
  if (getNewConfig()) {
    printf("New Config\n");
    saveConfig();
  } else {
    printf("Old Config\n");
    loadConfig();
  }
  printf("Load completed\n");
  return configs;
}
