#include "gdbstub.h"
#include <bios.h>
#include <dpmi.h>
#include <stdio.h>

void set_mode(int mode) {
	__dpmi_regs regs;

	memset(&regs, 0, sizeof regs);
	regs.x.ax = mode;
	__dpmi_int(0x10, &regs);
}

void inc(int *i) {
	i++;
}

int main(void) {
	gdb_start();
	set_mode(0x13);
	int j = 0;
	for (int i = 0; i < 1000000000; i++) {
		inc(&j);
	}
	printf("%i\n", j);

	set_mode(0x3);
	return 0;
}