extern "C" {
#include <linux/membarrier.h>
}

#pragma message("membarrier test")

int main() {
    int x = MEMBARRIER_CMD_PRIVATE_EXPEDITED | MEMBARRIER_CMD_REGISTER_PRIVATE_EXPEDITED;
    (void)x;
    return 0;
}
