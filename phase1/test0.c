#include "phase1.h"
#include <stdio.h>
#include <stdlib.h>


int Child(void *arg) {
    USLOSS_Console("Child\n");
    USLOSS_Console("Abby HERE\n");
    return 0;
}

int P2_Startup(void *notused) 
{
    USLOSS_Console("P2_Startup\n");
    P1_Fork("Child", Child, NULL, USLOSS_MIN_STACK, 2);
    return 0;
}


void setup(void) {
    // Do nothing.
}

void cleanup(void) {
    // Do nothing.
}