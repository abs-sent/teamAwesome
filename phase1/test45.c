#include "phase1.h"
#include "usloss.h"
#include <stdio.h>
#include <stdlib.h>

int Child(){
	return 0;
}
int P2_Startup(void *notused) 
{
    USLOSS_Console("P2_Startup\n");

	int i = 5;
	while(i--){
		P1_Fork("Child", Child, NULL, 4*USLOSS_MIN_STACK, 3);
	}
    return 0;
}

void setup(void) {
    // Do nothing.
}

void cleanup(void) {
    // Do nothing.
}
