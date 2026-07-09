#include <stdio.h>

#include "remon.h"

#define ITERS 4096
#define ALLOC_SIZE 512*1024

int main (){
	remon_vmm vmm;
	void *a = NULL;
	void *b = NULL;

	void *ptrs[ITERS];

	for (int i = 0; i < ITERS; i++) {
		printf("alloc iteration %d\n", i);
		ptrs[i] = vmm.remon_malloc(ALLOC_SIZE);

		if (i % 900 == 899) {
			// sleep for a while
			//sleep(30);
		}
	}

	for (int i = 0; i < ITERS; i++) {
		printf("free iteration %d\n", i);
		vmm.remon_free(ptrs[i]);
	}

	a = vmm.remon_malloc(1024);
	b = vmm.remon_malloc(2048);

	vmm.remon_free(a);
	vmm.remon_free(b);

	printf("Done!\n");

	return 0;
}
