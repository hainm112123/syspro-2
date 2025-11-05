#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <windows.h>   // for Sleep()
#include <process.h>   // for _getpid()
#include <time.h>      // for rand()

#define MEM_SIZE_MB 100
#define DISK_FILE "stress_test.tmp"
#define BUF_SIZE 4096

volatile int running = 1;

// CPU load thread
DWORD WINAPI cpu_worker(LPVOID arg) {
  while (running) {
    double x = 0;
    for (int i = 0; i < 1000000; i++)
      x += sin(i) * tan(i);
    Sleep(1);  // replace usleep(1000)
  }
  return 0;
}

// Memory load thread
DWORD WINAPI mem_worker(LPVOID arg) {
  size_t size = MEM_SIZE_MB * 1024 * 1024;
  char* mem = malloc(size);
  if (!mem) { perror("malloc"); return 0; }

  while (running) {
    for (size_t i = 0; i < size; i += 4096)
      mem[i] = (char)(mem[i] + 1);
    Sleep(10);  // replace usleep(10000)
  }
  free(mem);
  return 0;
}

// Disk I/O thread
DWORD WINAPI disk_worker(LPVOID arg) {
  FILE* f = fopen(DISK_FILE, "w+b");
  if (!f) { perror("fopen"); return 0; }

  char* buf = malloc(BUF_SIZE);
  for (int i = 0; i < BUF_SIZE; i++) buf[i] = rand() % 256;

  while (running) {
    fwrite(buf, 1, BUF_SIZE, f);
    fflush(f);
    fseek(f, 0, SEEK_SET);
    fread(buf, 1, BUF_SIZE, f);
  }

  fclose(f);
  free(buf);
  return 0;
}

int main() {
  HANDLE cpu_t = CreateThread(NULL, 0, cpu_worker, NULL, 0, NULL);
  HANDLE mem_t = CreateThread(NULL, 0, mem_worker, NULL, 0, NULL);
  HANDLE disk_t = CreateThread(NULL, 0, disk_worker, NULL, 0, NULL);

  printf("Running stress test... PID=%d\n", _getpid());
  printf("Press Ctrl+C to stop.\n");

  for (int i = 0; i < 300; i++) Sleep(1000);  // 30 seconds
  running = 0;

  WaitForSingleObject(cpu_t, INFINITE);
  WaitForSingleObject(mem_t, INFINITE);
  WaitForSingleObject(disk_t, INFINITE);

  CloseHandle(cpu_t);
  CloseHandle(mem_t);
  CloseHandle(disk_t);

  printf("Done.\n");
  return 0;
}
