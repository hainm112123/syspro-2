#include "utils.h"

#ifdef _WIN32
std::wstring stringToWstring(const std::string& str) {
  if (str.empty()) return L"";
  int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
  std::wstring wstr(size_needed, 0);
  MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], size_needed);
  wstr.pop_back();
  return wstr;
}
std::string wstringToString(const std::wstring& wstr) {
  int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
  std::string str(size_needed, 0);
  WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], size_needed, NULL, NULL);
  str.pop_back();
  return str;
}
#endif