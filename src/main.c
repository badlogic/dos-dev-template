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

int main(void) {
	gdb_start();

	set_mode(0x13);

	set_mode(0x3);
	return 0;
}