#include "IEventLogger.h"
#include <vector>
#include <fcntl.h>
#include <unistd.h>

class EventLogger: public IEventLogger {
private:
  int fd;
  std::vector<char> buffer;
public:
  EventLogger(const std::string path) {
    fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    buffer.reserve(BUF_SIZE);
  }
  ~EventLogger() {
    flush();
    if (fd >= 0) close(fd);
  }
  void log(const OveruseEvent &event);
  void flush();
};
