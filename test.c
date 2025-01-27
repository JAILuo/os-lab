#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <setjmp.h>
#include <assert.h>

void func1(int a, int b) {
    int c = 0;
    a++;
    b++;
    c = a + b;
}

int main() {

    func1(1, 2);

    return 0;
}

