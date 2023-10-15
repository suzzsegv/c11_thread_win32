#include <stdio.h>
#include "threads.h"

int main(int argc, void* argv[])
{
	thrd_lib_init();

	extern void c11ThreadTest(void);
	c11ThreadTest();
}
