extern "C" {
#include <signal.h>
#include <time.h>
}

#pragma message("rt_test test")


int main() {
    timer_t td;
    struct sigevent sev;
    timer_create(CLOCK_MONOTONIC, &sev, &td);
    return 0;
}
