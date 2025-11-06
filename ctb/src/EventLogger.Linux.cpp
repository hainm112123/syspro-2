#include "EventLogger.Linux.h"

void EventLogger::log(const OveruseEvent &event) {
  char line[256];
  int len = snprintf(line, sizeof(line), "{\"pid\":%d,\"name\":\"%s\",\"type\":%s,\"value\":%.2f,\"time\":%s}\n", 
                      event.pid, event.name.c_str(), event.type.c_str(), event.value, event.time.c_str());
  if (buffer.size() + len >= BUF_SIZE) flush();
  buffer.insert(buffer.end(), line, line + len);
}

void EventLogger::flush() {
  if (!buffer.empty()) {
    write(fd, buffer.data(), buffer.size());
    buffer.clear();
  }
}
