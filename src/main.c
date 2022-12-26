#include <bios.h>
#include <dpmi.h>
#include <sys/nearptr.h>
#include <stdlib.h>
#include <stdio.h>

#define GDB_IMPLEMENTATION
#include "gdbstub.h"

void set_video_mode(int mode) {
	__dpmi_regs regs = {0};
	regs.x.ax = mode;
	__dpmi_int(0x10, &regs);
}

int main(void) {
	gdb_start();

	// Go mode 0x13!
	set_video_mode(0x13);

	// Draw 200 pixels each iteration until a key is pressed
	while (!kbhit()) {
		__djgpp_nearptr_enable();
		unsigned char *vram = (unsigned char *) (__djgpp_conventional_base + 0xa0000);
		for (int i = 0; i < 200; i++) {
			int x = rand() % 320;
			int y = rand() % 200;
			int color = rand() % 255;
			vram[x + y * 320] = color;
		}
		__djgpp_nearptr_disable();

		// Set a GDB checkpoint, needed to receive interrupt commands
		// from the debugger. You should do this in all your game loops.
		gdb_checkpoint();
	}

	// Return to text mode
	set_video_mode(0x3);

	// Read an asset file and print its content
	FILE *file = fopen("assets/test.txt", "r");
	fseek(file, 0, SEEK_END);
	long int num_bytes = ftell(file);
	fseek(file, 0, SEEK_SET);
	unsigned char *file_content = (unsigned char *) malloc(num_bytes + 1);
	fread(file_content, 1, num_bytes, file);
	file_content[num_bytes] = 0;
	printf("%s\n", file_content);

	return 0;
}