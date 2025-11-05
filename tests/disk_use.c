// disk_write_test.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main() {
    FILE *f = fopen("disk_test.tmp", "wb");
    if (!f) {
        perror("fopen");
        return 1;
    }

    const size_t BUF_SIZE = 4096;
    char *buf = malloc(BUF_SIZE);
    if (!buf) return 1;

    for (size_t i = 0; i < BUF_SIZE; i++)
        buf[i] = rand() % 256;

    while (1) {
        fwrite(buf, 1, BUF_SIZE, f);
        fflush(f); // force write to disk
        usleep(10000); // slow it slightly (10ms)
    }

    fclose(f);
    free(buf);
    return 0;
}
