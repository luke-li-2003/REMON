#include <stdio.h>

#include "remon.h"

int main (){
	remon_vmm vmm;
	void *a = NULL;
	void *b = NULL;

	printf("allocating A\n");
	a = vmm.remon_malloc(1024);
	if (!a) {
		printf("allocation A failed\n");
		return 1;
	}

	printf("allocating B\n");
	b = vmm.remon_malloc(2048);
	if (!b) {
		printf("allocation B failed\n");
		vmm.remon_free(a);
		return 1;
	}

	printf("freeing A and B\n");
	vmm.remon_free(a);
	vmm.remon_free(b);

	printf("Done!\n");

	return 0;
}
