#include "IEventLogger.h"
#include <vector>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/eventfd.h>
#include <sys/un.h>
#include <poll.h>
#include <thread>
#include <chrono>

class EventLogger: public IEventLogger {
private:
  const std::string UnixSocketPath = "/tmp/eventfd.sock";
  int fd;
  std::vector<char> buffer;
  int eventfd;
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
  void getEventFd();
  void getEvents();
};
