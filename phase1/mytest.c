#include "phase1.h"
#include <stdio.h>
#include <stdlib.h>
P1_Semaphore a;
int Grandchild(void *arg) {
    USLOSS_Console("Grandchild\n");

    
    //P1_Fork("Grandchild", Child, NULL, USLOSS_MIN_STACK, 1);

    P1_P(a);
    USLOSS_Console("Grandchild finished\n");
    return 0;
}

int Child(void *arg) {
    USLOSS_Console("Child\n");
    a=P1_SemCreate(0);
    P1_V(a);
    P1_Fork("Grandchild", Grandchild, NULL, USLOSS_MIN_STACK, 1);


    USLOSS_Console("Child finished\n");
    return 0;
}

int P2_Startup(void *notused) 
{
    USLOSS_Console("P2_Startup\n");
    P1_Fork("Child", Child, NULL, USLOSS_MIN_STACK, 3);
    USLOSS_Console("P2_Startup finished\n");
    return 0;
}




void setup(void) {
    // Do nothing.
}

void cleanup(void) {
    // Do nothing.
}