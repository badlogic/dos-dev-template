#include <bios.h>
#include <stdio.h>

#include "gdb.h"

int main(void) {
    gdb_start();
    printf("Hello DOS\n");
    return 0;
}