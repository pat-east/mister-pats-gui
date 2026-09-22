#include <cstdio>
#include <unistd.h>

int main() {
    std::printf("Hello from a future MiSTer replacement, built on an M1 Mac.\n");
    std::printf("PID=%d\n", static_cast<int>(getpid()));
    return 0;
}
