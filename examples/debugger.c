

#include "pc.h"
int foo(int a, int b) {
	for (int i = 0; i < 10; i++) {
		a += 12;
		b += a * i;
	}
	return a + b;
}

int main(void) {
	while (!kbhit()) {
		int result = foo(1, 2);
		printf("result: %i\n", result);
	}
}