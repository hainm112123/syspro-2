#include <iostream>
#include <vector>
#include <thread>
#include <chrono>

int main() {
    const size_t MB = 1024 * 1024;
    size_t alloc_mb = 500;  // allocate 500 MB

    std::vector<char*> blocks;

    for (size_t i = 0; i < alloc_mb; ++i) {
        char* block = new char[MB];  // allocate 1 MB
        std::fill(block, block + MB, 0);
        blocks.push_back(block);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));  // slow ramp-up
    }

    std::cout << "Allocated " << alloc_mb << " MB. Sleeping...\n";
    std::this_thread::sleep_for(std::chrono::seconds(60));  // stay alive for monitoring

    // Cleanup
    for (auto b : blocks) delete[] b;
}
