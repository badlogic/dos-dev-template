#include "pc.h"
#include "sys/segments.h"
#include "unistd.h"
#include <dpmi.h>
#include <stdio.h>
#include <stdint.h>

// #define GDB_DEBUG_PRINT
#include "../src/gdbstub.h"

#define KEY_EXTENDED 0x60
#define KEY_LEFT (KEY_EXT_SHIFT + 75)
#define KEY_RIGHT (KEY_EXT_SHIFT + 77)
#define KEY_UP (KEY_EXT_SHIFT + 72)
#define KEY_DOWN (KEY_EXT_SHIFT + 80)
#define KEY_ESC 1

#define KEYBOARD_INTERRUPT 0x9
#define PAGE_SIZE 4096;

typedef struct keyboard_state {
	_go32_dpmi_seginfo old_keyboard_handler;
	_go32_dpmi_seginfo new_keyboard_handler;
	uint8_t keys[0xc0];
	uint8_t extended_key;
} keyboad_state;
keyboad_state keyboard;

void keyboard_handler() {
	uint8_t raw_code = inp(0x60);
	uint8_t pressed = !(raw_code & 0x80);
	int scan_code = raw_code & 0x7F;

	if (keyboard.extended_key == 0xE0) {
		if (scan_code < 0x60) {
			keyboard.keys[KEY_EXTENDED + scan_code] = pressed;
		}
		keyboard.extended_key = 0;
	} else if (keyboard.extended_key >= 0xE1 && keyboard.extended_key <= 0xE2) {
		keyboard.extended_key = 0;
	} else if (raw_code >= 0xE0 && raw_code <= 0xE2) {
		keyboard.extended_key = raw_code;
	} else if (scan_code < 0x60) {
		keyboard.keys[scan_code] = pressed;
	}

	outportb(0x20, 0x20);
}

void setup_keyboard() {
	_go32_dpmi_lock_data(&keyboard, sizeof(keyboard));
	_go32_dpmi_lock_code(keyboard_handler, 4096);

	_go32_dpmi_get_protected_mode_interrupt_vector(KEYBOARD_INTERRUPT, &keyboard.old_keyboard_handler);

	keyboard.new_keyboard_handler.pm_offset = (int) keyboard_handler;
	keyboard.new_keyboard_handler.pm_selector = _my_cs();

	_go32_dpmi_allocate_iret_wrapper(&keyboard.new_keyboard_handler);
	_go32_dpmi_set_protected_mode_interrupt_vector(KEYBOARD_INTERRUPT, &keyboard.new_keyboard_handler);
}

void reset_keyboard() {
	_go32_dpmi_set_protected_mode_interrupt_vector(KEYBOARD_INTERRUPT, &keyboard.old_keyboard_handler);
	_go32_dpmi_free_iret_wrapper(&keyboard.new_keyboard_handler);
}

int main(void) {
	printf("keyboard\n");
	gdb_start();//
	setup_keyboard();
	printf("Set up keyboard\n");

	while (!keyboard.keys[KEY_ESC]) {
		uint8_t key_pressed = 0;
		for (int i = 0; i < 0xc0; i++) {
			if (keyboard.keys[i]) {
				printf("%i, ", i);
				key_pressed = 1;
			}
		}
		if (key_pressed) printf("\n");
		gdb_checkpoint();
	}

	reset_keyboard();

	return 0;
}