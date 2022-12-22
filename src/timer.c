#include <pc.h>
#include <stdio.h>
#include "gdbstub.h"

int main(void) {
	gdb_start();

	//notice no changes to counter in this loop- the interrupt
	//changes it
	int sum = 0;
	while (!kbhit()) {
		sum += 1;
	}
	printf("%i\n", sum);

	return 0;
}