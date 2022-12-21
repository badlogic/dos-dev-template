#include <bios.h>
#include <stdio.h>
#include "gdbstub.h"

int main(void) {
    gdb_start();

    printf("Hello DOS\n");
    return 0;
}