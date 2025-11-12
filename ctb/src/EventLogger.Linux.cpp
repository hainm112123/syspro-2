#include "EventLogger.Linux.h"

void EventLogger::log(const OveruseEvent &event) {
  printf("{\"pid\":%u,\"name\":\"%s\",\"type\":%s,\"value\":%.2f,\"time\":%s}\n", 
                      event.pid, event.name, event.type, event.value, event.time);
                      
  char line[256];
  int len = snprintf(line, sizeof(line), "{\"pid\":%u,\"name\":\"%s\",\"type\":%s,\"value\":%.2f,\"time\":%s}\n", 
                      event.pid, event.name, event.type, event.value, event.time);
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
  strncpy(addr.sun_path, UNIX_SOCKET_PATH.c_str(), sizeof(addr.sun_path) - 1);

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
  // char buf[1];
  // struct iovec io = {.iov_base = buf, .iov_len = sizeof(buf)};
  // msg.msg_iov = &io;
  // msg.msg_iovlen = 1;

  char c_buf[CMSG_SPACE(sizeof(int))];
  msg.msg_control = c_buf;
  msg.msg_controllen = sizeof(c_buf);

  if (recvmsg(socketfd, &msg, 0) < 0) {
    perror("Get event fd");
    return;
  }

  struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
  if (cmsg && cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
    // eventFd = *((int *) CMSG_DATA(cmsg));
    memcpy(&eventFd, CMSG_DATA(cmsg), sizeof(eventFd));
    printf("Event fd received: %d\n", eventFd);
  }

  close(socketfd);
}

void EventLogger::getEvents() {
  // printf("Waiting for signal...\n");
  uint64_t count;
  read(eventFd, &count, sizeof(count));
  while (count --) {
    uint32_t head = __atomic_load_n(&ring->head, __ATOMIC_RELAXED);
    uint32_t tail = __atomic_load_n(&ring->tail, __ATOMIC_ACQUIRE);
    printf("Head: %d, Tail: %d\n", head, tail);
    if (head == tail) {
      break;
    }
    auto event = ring->buffer[head];
    log(event);
    __atomic_store_n(&ring->head, (head + 1) & (RING_SIZE - 1), __ATOMIC_RELEASE);
  }
}

void EventLogger::run() {
  getEventFd();
	
  mmapFd = shm_open(UNIX_EVENT_PATH.c_str(), O_CREAT | O_RDWR, 0666);
  bool isCreator = (errno != EEXIST);
  if (isCreator) ftruncate(mmapFd, sizeof(SharedRing));
  ring = (SharedRing *) mmap(nullptr, sizeof(SharedRing), PROT_READ | PROT_WRITE, MAP_SHARED, mmapFd, 0);

  printf("Start waiting for events...\n");
	printf("Press q to quit...\n");
	while (true) {
		fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    struct timeval tv = {0L, 0L};
    int res = select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv);
    if (res > 0 && FD_ISSET(STDIN_FILENO, &fds)) {
      char input;
      read(STDIN_FILENO, &input, 1);
      if (input == 'q') {
        break;
      }
    }
    getEvents();
    std::this_thread::sleep_for(std::chrono::seconds(1));
	}
  flush();
  munmap(ring, sizeof(SharedRing));
  if (eventFd >= 0) close(eventFd);
  if (mmapFd >= 0) close(mmapFd);
  if (isCreator) shm_unlink(UNIX_EVENT_PATH.c_str());
}
