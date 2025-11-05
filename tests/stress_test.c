// stress_test.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <math.h>

#define MEM_SIZE_MB 100   // memory to allocate
#define DISK_FILE "stress_test.tmp"
#define BUF_SIZE 4096

volatile int running = 1;

// CPU load thread
void* cpu_worker(void* arg) {
    while (running) {
        double x = 0;
        for (int i = 0; i < 1000000; i++)
            x += sin(i) * tan(i);
        usleep(1000);
    }
    return NULL;
}

// Memory load thread
void* mem_worker(void* arg) {
    size_t size = MEM_SIZE_MB * 1024 * 1024;
    char *mem = malloc(size);
    if (!mem) {
        perror("malloc");
        return NULL;
    }

    // fill + modify memory repeatedly
    while (running) {
        for (size_t i = 0; i < size; i += 4096)
            mem[i] = (char)(mem[i] + 1);
        usleep(10000);
    }
    free(mem);
    return NULL;
}

// Disk I/O thread
void* disk_worker(void* arg) {
    FILE *f = fopen(DISK_FILE, "w+b");
    if (!f) {
        perror("fopen");
        return NULL;
    }

    char *buf = malloc(BUF_SIZE);
    for (int i = 0; i < BUF_SIZE; i++)
        buf[i] = rand() % 256;

    while (running) {
        fwrite(buf, 1, BUF_SIZE, f);
        fflush(f);
        fseek(f, 0, SEEK_SET);
        fread(buf, 1, BUF_SIZE, f);
        // usleep(10);
    }

    fclose(f);
    free(buf);
    return NULL;
}

int main() {
    pthread_t cpu_t, mem_t, disk_t;
    pthread_create(&cpu_t, NULL, cpu_worker, NULL);
    pthread_create(&mem_t, NULL, mem_worker, NULL);
    pthread_create(&disk_t, NULL, disk_worker, NULL);

    printf("Running stress test... PID=%d\n", getpid());
    printf("Press Ctrl+C to stop.\n");

    // Run for ~30s (or infinite if you want)
    for (int i = 0; i < 300; i++) sleep(1);
    running = 0;

    pthread_join(cpu_t, NULL);
    pthread_join(mem_t, NULL);
    pthread_join(disk_t, NULL);

    printf("Done.\n");
    return 0;
}
