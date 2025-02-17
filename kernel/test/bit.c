#include <stdio.h>


int main() {
    int a = 0x302000;

    printf("a ^ (1 << 9): 0x%x\n", a ^(1 << 9));
    printf("a ^ (1 << 8): 0x%x\n", a ^(1 << 8));
    printf("a ^ (1 << 7): 0x%x\n", a ^(1 << 7));
    printf("a ^ (1 << 6): 0x%x\n", a ^(1 << 6));


    return 0;
}
