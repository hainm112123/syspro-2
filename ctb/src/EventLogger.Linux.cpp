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

void EventLogger::getEventFd() {
  int socketfd = socket(AF_UNIX, SOCK_STREAM, 0);
  struct sockaddr_un addr = {0};
  addr.sun_family = AF_UNIX;
  // printf("cp 0\n");
  strncpy(addr.sun_path, UnixSocketPath.c_str(), sizeof(addr.sun_path) - 1);

  if (socketfd < 0) {
    perror("Get eventfd: socketfd");
    return;
  }
  // printf("cp 1\n");

  while (connect(socketfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
    if (errno == ENOENT || errno == ECONNREFUSED) {
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
      continue;
    } else {
      perror("Connect to get event fd");
      return;
    }
  }
  // printf("cp 2\n");
  
  struct msghdr msg = {0};
  char buf[1];
  struct iovec io = {.iov_base = buf, .iov_len = sizeof(buf)};
  msg.msg_iov = &io;
  msg.msg_iovlen = 1;

  char c_buf[CMSG_SPACE(sizeof(int))];
  msg.msg_control = c_buf;
  msg.msg_controllen = sizeof(c_buf);

  if (recvmsg(socketfd, &msg, 0) < 0) {
    perror("Get event fd");
    return;
  }

  struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
  eventfd = *((int *) CMSG_DATA(cmsg));
  printf("Event fd received: %d\n", eventfd);

  close(socketfd);
}
