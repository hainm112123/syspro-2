// cpu_busy.c
#include <unistd.h>
#include <math.h>

int main() {
    while (1) {
        // Burn some CPU cycles
        for (volatile int i = 0; i < 1000000; i++);
        usleep(1000); // optional small sleep to avoid 100% CPU
    }
}
