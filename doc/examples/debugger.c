#include "pc.h"
#include <stdio.h>
#include <stdlib.h>

// To add the code of the debugger to your program, define
// GDB_IMPLEMENTATION in one .c or .cpp file and include
// 'gdbstub.h'
#define GDB_IMPLEMENTATION
#include "../../src/gdbstub.h"

int foo(int a, int b) {
	for (int i = 0; i < 10; i++) {
		a += 12;
		b += a * i;
	}
	return a + b;
}

int main(void) {
	// To start the debugger, call gdb_start()
	gdb_start();

	while (!kbhit()) {
		int a = rand();
		int b = rand();
		printf("%i\n", foo(a, b));

		// To give the debugger a chance to interrupt your program
		// call 'gdb_checkpoint()' somewhere in your main loop.
		gdb_checkpoint();
	}
}