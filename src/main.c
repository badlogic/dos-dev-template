#include <bios.h>
#include <stdio.h>
#include "gdbstub.h"

typedef struct point {
    float x, y;
} point;

int main(void) {
    gdb_start();

    point p = { 1, 2 };
    p.x = 34;
    p.y = p.x + 2;

    printf("Hello DOS, %f, %f\n", p.x, p.y);
    return 0;
}