 /*
 ------------------------------------------------------------------------
   Team Check Your Privilege
   Members:
   Joshua Redpath
   Abigail Arias
   Skeleton file for Phase 1. These routines are very incomplete and are
   intended to give you a starting point. Feel free to use thiTeams or not.
   ------------------------------------------------------------------------ 
   */

#include <stddef.h>
#include <stdlib.h>
#include "usloss.h"
#include "phase1.h"
#include <string.h>

#define DEFAULT -99
#define FIRST_RUN -98
/* -------------------------- Globals ------------------------------------- */

typedef struct PCB {
    USLOSS_Context      context;
    int                 (*startFunc)(void *);   /* Starting function */
    void                 *startArg;             /* Arg to starting function */
    int PID;
    int cpuTime;
    int lastStartedTime;
    int isOrphan;
    int state;//0=running, 1=ready,2=killed,3=quit,4=waiting
    int status;
    int numChildren;
    int parent;
    int priority;
    char* name;
    void* stack;
    int notFreed;
    struct PCB* nextPCB;
    struct PCB* prevPCB;    
} PCB;

typedef struct 
{
  int value;
  struct PCB *queue;
}Semaphore;

int dispatcherTimeTracker=-1;
/* the process table */
PCB procTable[P1_MAXPROC];

/* the semaphore table */
Semaphore semTable[P1_MAXSEM];


PCB readyHead;
PCB blockedHead;
PCB quitListHead;
/* current process ID */
int currPid = -1;

/* number of processes */

int numProcs = 0;

static int sentinel(void *arg);
static void launch(void);
static void removeProc(int PID);
static void Check_Your_Privilege();
static void free_Procs();

//static int P1_ReadTime(void);

P1_Semaphore P1_SemCreate(unsigned int value);
int P1_SemFree(P1_Semaphore sem);
int P1_P(P1_Semaphore sem);
int P1_V(P1_Semaphore sem);


/* -------------------------- Functions ----------------------------------- */
/* ------------------------------------------------------------------------
   Name - dispatcher
   Purpose - runs the highest priority runnable process
   Parameters - none
   Returns - nothing
   Side Effects - runs a process
   ----------------------------------------------------------------------- */
int P1_WaitDevice(int type, int unit, int *status){
  if(procTable[currPid].state==2){//Checks if killed
    return -3;
  }
  /*TO DO: Check If valid Unit*/
  if(unit<0||unit>3){
    return -1;
  }
  switch(type){
    case 0:
      // P1_P(type_semaphore);
      break;
    case 1:
      // P1_P(type_semaphore);
      break;
    case 2:
      // P1_P(type_semaphore);
      break;
    case 3:
      // P1_P(type_semaphore);
      break;
    default:
      return -2;//Invalid Type
  }
  
  return 0;
}

void tempSyscallHandler(){
  USLOSS_Console("System call %d not implemented",currPid);
  P1_Quit(1);
}

void freeProc(int PID){
  free(procTable[PID].stack); 
  free(procTable[PID].name);
  procTable[PID].notFreed = 0; 
}

void free_Procs(){
  int i;
  for( i= 0; i <P1_MAXPROC; i++){
    if(procTable[i].state == 3 && procTable[i].notFreed) {
      freeProc(i);
      if(procTable[i].isOrphan){
        removeProc(i);
      }
      //USLOSS_Console("Freed %s\n",procTable[i].name);
    }
  }
}
/*Prints out Linked List*/
void printList(PCB* listHead){
  if(listHead->nextPCB==NULL){
    USLOSS_Console("empty list\n");
    listHead=listHead->nextPCB;
    return;
  }
  int i=0;
  while(listHead->nextPCB!=NULL){
    listHead=listHead->nextPCB;
    i++;
    USLOSS_Console("PCB %d->",listHead->PID);
  }
  USLOSS_Console("\nList Size: %d\n",i);
}

void addToReadyList(int PID){
  PCB* pos = &readyHead;
    while(pos->nextPCB!=NULL&&pos->nextPCB->priority<=procTable[PID].priority){
      pos=pos->nextPCB;
    }
    if(pos->nextPCB==NULL){//end of list
      pos->nextPCB=&procTable[PID];
      procTable[PID].prevPCB=pos;
      procTable[PID].nextPCB=NULL;
    }else{//middle of list
      procTable[PID].prevPCB=pos;
      procTable[PID].nextPCB=pos->nextPCB;
      pos->nextPCB->prevPCB=&procTable[PID];
      pos->nextPCB=&procTable[PID];
    }
}

void addToBlockedList(int PID){
  /*Add to blocked List*/
  PCB* pos=&blockedHead;
  while(pos->nextPCB&&pos->nextPCB->priority<procTable[PID].priority){
    // USLOSS_Console("Looping on %s\n",pos->nextPCB->name);
    pos=pos->nextPCB;
  }
  pos->nextPCB=&procTable[PID];
  procTable[PID].nextPCB=NULL;
  procTable[PID].prevPCB=pos;
}

void addToQuitList(int PID){
  PCB* pos=&quitListHead;
  while(pos->nextPCB!=NULL){
    pos=pos->nextPCB;
  }
  pos->nextPCB=&procTable[PID];
  procTable[PID].nextPCB=NULL;
  procTable[PID].prevPCB=pos;
}

void addToQue(int PID,PCB* list){
  PCB* pos=list;
  while(pos->nextPCB!=NULL){
    pos=pos->nextPCB;
  }
  pos->nextPCB=&procTable[PID];
  procTable[PID].nextPCB=NULL;
  procTable[PID].prevPCB=pos;
  // USLOSS_Console("Queue after addtoQue: ");
  // printList(list);
}

void removeFromList(int PID){
  // USLOSS_Console("RFL PCB %d\n",PID);
  // USLOSS_Console("ReadyList B4:");
  // printList(&readyHead);
  // USLOSS_Console("QuitList:");
  // printList(&quitListHead);
  if(procTable[PID].nextPCB!=NULL){
    procTable[PID].nextPCB->prevPCB=procTable[PID].prevPCB;
  }
  procTable[PID].prevPCB->nextPCB=procTable[PID].nextPCB;
  procTable[PID].nextPCB=NULL;
  procTable[PID].prevPCB=NULL;
  // USLOSS_Console("ReadyList After:");
  // printList(&readyHead);
}

void dispatcher()
{
  // USLOSS_Console("dispatcher");
  // printList(&readyHead);
  Check_Your_Privilege();
  // USLOSS_Console("dispatcher Called\n");
  /*Adjust Runttime for current process*/
  int timeRun;
  if (procTable[currPid].lastStartedTime==FIRST_RUN) {
    timeRun=0;
  }else{
      timeRun=USLOSS_Clock()-procTable[currPid].lastStartedTime;
  }
  procTable[currPid].cpuTime+=timeRun;
  /*
   * Run the highest priority runnable process. There is guaranteed to be one
   * because the sentinel is always runnable.
   */
  // USLOSS_Console("In dispatcher PID -- before:  %d\n", currPid);
  int oldpid = currPid;
  PCB* readyListPos=&readyHead;
  // while(readyListPos->nextPCB){//List is in order of priority
  //   /*Breaks and runs this process*/
  //   if(readyListPos->nextPCB->state==1||readyListPos->nextPCB->state==2){
  //       break;
  //   }
  //   // USLOSS_Console("Moving Foward in dispatcher\n");
  //   readyListPos=readyListPos->nextPCB;
  // }


  readyListPos->nextPCB->state=0;//set state to running
  /*Set Proc state to ready unless it has quit or been killed*/
  if(procTable[oldpid].state!=3||procTable[oldpid].state==2){
    procTable[oldpid].state=1;
  }
  currPid = readyListPos->nextPCB->PID;
  procTable[currPid].lastStartedTime=USLOSS_Clock();
  /*Adds currpid to end of its priority section in the Rdy list*/
  removeFromList(currPid);
  addToReadyList(currPid);

 // USLOSS_Console("In dispatcher PID -- after:  %d\n", currPid); 
  USLOSS_ContextSwitch(&(procTable[oldpid]).context, &(readyListPos->nextPCB->context));

}

/* ------------------------------------------------------------------------
   Name - startup
   Purpose - Initializes semaphores, process lists and interrupt vector.
             Start up sentinel process and the P2_Startup process.
   Parameters - none, called by USLOSS
   Returns - nothing
   Side Effects - lots, starts the whole thing
   ----------------------------------------------------------------------- */
void startup()
{
  Check_Your_Privilege();
  int i;
  /* initialize the process table here */
  for(i = 0; i < P1_MAXPROC; i++){
      PCB dummy;
      procTable[i]=dummy;
      //USLOSS_Context DummyCon;
      procTable[i].priority = -1;
      //procTable[i].context=DummyCon;
  }

  /*initialize the semaphore table*/
  for(i=0; i< P1_MAXSEM; i++){
    Semaphore dummy;
    PCB listHead;
    dummy.queue=&listHead;
    semTable[i] = dummy;
    semTable[i].value = -1;
  } 

  /* Initialize the Ready list, Blocked list, etc. here */
  readyHead.prevPCB=NULL;
  readyHead.nextPCB=NULL;
  blockedHead.prevPCB=NULL;
  blockedHead.nextPCB=NULL;
  quitListHead.nextPCB=NULL;
  quitListHead.prevPCB=NULL;
  /* Initialize the interrupt vector here */
  USLOSS_IntVec[USLOSS_CLOCK_INT] = NULL;
  USLOSS_IntVec[USLOSS_ALARM_INT] = NULL;
  USLOSS_IntVec[USLOSS_DISK_INT] = NULL;
  USLOSS_IntVec[USLOSS_TERM_INT] = NULL;
  USLOSS_IntVec[USLOSS_MMU_INT] = NULL;
  USLOSS_IntVec[USLOSS_SYSCALL_INT] = &tempSyscallHandler;
  /* Initialize the semaphores here */
  
  /* startup a sentinel process */
  /* HINT: you don't want any forked processes to run until startup is finished.
   * You'll need to do something in your dispatcher to prevent that from happening.
   * Otherwise your sentinel will start running right now and it will call P1_Halt. 
   */
  P1_Fork("sentinel", sentinel, NULL, USLOSS_MIN_STACK, 6);

  /* start the P2_Startup process */
  P1_Fork("P2_Startup", P2_Startup, NULL, 4 * USLOSS_MIN_STACK, 1);

  dispatcher();

  /* Should never get here (sentinel will call USLOSS_Halt) */

  return;
} /* End of startup */

/* ------------------------------------------------------------------------
   Name - finish
   Purpose - Required by USLOSS
   Parameters - none
   Returns - nothing
   Side Effects - none
   ----------------------------------------------------------------------- */
void finish()
{
  Check_Your_Privilege();
  // free_Procs();
  USLOSS_Console("Goodbye.\n");
} /* End of finish */


int P1_GetPID(){
  Check_Your_Privilege();
  return currPid;
}

int P1_GetState(int PID){
  Check_Your_Privilege();
  if(PID<0||PID>P1_MAXPROC-1){
    return -1;
  }else if(PID==currPid){
    return 0;
  }
 // USLOSS_Console("In P1_GetState PID -- after:  %d,State=%d\n", PID,procTable[PID].state);
  return procTable[PID].state;//1=ready,2=killed,3=quit,4=waiting
}


void P1_DumpProcesses(){// Do CPU Time Part
  Check_Your_Privilege();
  USLOSS_Console("Dumping Process\n");
  int i;
    for(i=0;i<P1_MAXPROC;i++){
        if(procTable[i].priority==-1){
          continue;
        }
        int state=procTable[i].state;
        char* statePhrase;
        switch(state){
          case 1:
            statePhrase="Ready";
            break;
          case 2:
            statePhrase="Killed";
            break;
          case 3:
            statePhrase="Quit";
            break;
          case 4:
            statePhrase="Waiting";
            break;
        }
        int cpu;
        if(i==currPid){
          cpu=P1_ReadTime();
        }else{
          cpu=procTable[i].cpuTime;
        }
        USLOSS_Console("Name:%s\t PID:%-5d\tParent:%d\tPriority:%d\tState:%s\tKids:%d\tCPUTime:%d\n",
                procTable[i].name,i,procTable[i].parent,procTable[i].priority,
                statePhrase,procTable[i].numChildren,cpu); 
    }
}

int P1_ReadTime(void){
    return USLOSS_Clock()-procTable[currPid].lastStartedTime+procTable[currPid].cpuTime;
}

/*Checks Whether or not thecurrent process is in Kernel Mode*/
void Check_Your_Privilege(){
  if((USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE)==0){
    USLOSS_Console("Error: Access Denied to User Mode");
    USLOSS_Halt(1);
  }
}


P1_Semaphore P1_SemCreate(unsigned int value){

  P1_Semaphore semPointer; 
  // put the semaphore in the table
  int i = 0;
  while(semTable[i].value != -1 && i < P1_MAXSEM){
    i++;
  }
  semTable[i].value = value;
  semTable[i].queue->nextPCB=NULL;
  semTable[i].queue->prevPCB=NULL;
  semPointer = &semTable[i];
  return semPointer;
}

int P1_SemFree(P1_Semaphore sem){
  Check_Your_Privilege();
  //if sem is invalid return -1, sem is invalid if it is not created using SemCreate method
  Semaphore* semP=(Semaphore*)sem;
  if(semP->value < 0 && semP->value > P1_MAXSEM-1){
    USLOSS_Console("Semaphore is invalid\n");
    USLOSS_Halt(1);
    return -1;
  }
  free(sem);
  return 0;
}

int P1_P(P1_Semaphore sem){
  // USLOSS_Console("P1_P running\n");
  Check_Your_Privilege();
  Semaphore* semP=(Semaphore*)sem;

  // check if the process is killed
  if(procTable[currPid].state == 2){
    return -2;
  }
  // check if the semaphore is valid
  if(semP->value < 0 || semP->value > P1_MAXSEM-1){
    USLOSS_Console("Semaphore is invalid\n");
    return - 1;
  }
  while(1){
    // interrupt disable HERE;
    if(semP->value > 0){
      semP->value--;
      break;
    }
    if(currPid!=-1){
      procTable[currPid].state=4;
      removeFromList(currPid);
      addToQue(currPid,semP->queue);
      // USLOSS_Console("P() queue:");
      // printList(semP->queue);
    }
  }
  //interrupt enable
  return 0;
}

int P1_V(P1_Semaphore sem){
  // USLOSS_Console("P1_V for PCB %d running\n",currPid);
  Check_Your_Privilege();
  // interrupt disable HERE!
  Semaphore* semP=(Semaphore*)sem;
  
  // check if the semaphore is valid
  if(semP->value < 0 || semP->value > P1_MAXSEM-1){
    USLOSS_Console("Semaphore is invalid\n");
    return - 1;
  }
  semP->value++;

  if(currPid>=0&&semP->queue->nextPCB != NULL){
    //addToReady
    int PID=semP->queue->nextPCB->PID;
    // USLOSS_Console("PCB>> %s\n",procTable[currPid].name);
    removeFromList(PID);
    addToReadyList(PID);
    procTable[PID].state=1;
    dispatcher();
  }
  // USLOSS_Console("P1_V ending\n");
  // interrupt enable HERE!
  return 0;
}





/* ------------------------------------------------------------------------
   Name - P1_Fork
   Purpose - Gets a new process from the process table and initializes
             information of the process.  Updates information in the
             parent process to reflect this child process creation.
   Parameters - the process procedure address, the size of the stack and
                the priority to be assigned to the child process.
   Returns - the process id of the created child or an error code.
   Side Effects - ReadyList is changed, procTable is changed, Current
                  process information changed
   ------------------------------------------------------------------------ */
int P1_Fork(char *name, int (*f)(void *), void *arg, int stacksize, int priority)
{
    /*Check current Mode. If not Kernel Mode return error*/
    Check_Your_Privilege();

    //free the available spots
    free_Procs();

    /*Check Priority and Stack Size*/
    if(priority<1||priority>6){//is priority # valid
      return -3;
    }
    if(stacksize<USLOSS_MIN_STACK){//is stacksize valid
      return -2;
    }
    
    //find PID
    int newPid = 0;
    while(procTable[newPid].priority!=-1){
      newPid++;
      if(newPid>=P1_MAXPROC){
        return -1;
      }
    }

    P1_Semaphore sema=P1_SemCreate(1);
    P1_P(sema);

    /* stack = allocated stack here */
    procTable[newPid].stack=malloc(stacksize*sizeof(char));
    procTable[newPid].notFreed=1;

    /*set PCB fields*/
    procTable[newPid].PID=newPid;
    procTable[newPid].cpuTime=0;
    procTable[newPid].lastStartedTime=FIRST_RUN;
    procTable[newPid].state=1;//0=running 1=ready,2=killed,3=quit,4=waiting

    procTable[newPid].status=DEFAULT;

    if(currPid==-1){
      procTable[newPid].isOrphan=1;
    }
    procTable[newPid].parent=currPid;
    procTable[currPid].numChildren++;//increment parents numChildren
    procTable[newPid].numChildren=0;
    procTable[newPid].priority=priority;
    procTable[newPid].name=strdup(name);
    procTable[newPid].startFunc = f;
    procTable[newPid].startArg = arg;
    procTable[newPid].isOrphan= 0;
    /*PCB Fields are set*/


    /*add to ready list*/
    addToReadyList(newPid);    

    /*increment numProcs*/
    numProcs++;
    
    /*initialize context*/
    USLOSS_ContextInit(&(procTable[newPid].context), USLOSS_PsrGet(), procTable[newPid].stack, 
        stacksize, launch);

    P1_V(sema);
    /*Run dispatcher if forking higher priority process*/
    if(currPid != -1&&priority<procTable[currPid].priority){
      dispatcher();
    }
    //USLOSS_Console("In Fork PID -- after:  %d\n", currPid);
    return newPid;
} /* End of fork */

/* ------------------------------------------------------------------------
   Name - launch
   Purpose - Dummy function to enable interrupts and launch a given process
             upon startup.
   Parameters - none
   Returns - nothing
   Side Effects - enable interrupts
   ------------------------------------------------------------------------ */
void launch(void){
  Check_Your_Privilege();
  int  rc;
  USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);
  rc = procTable[currPid].startFunc(procTable[currPid].startArg);
  // USLOSS_Console("Laung Ending for %s\n",procTable[currPid].name);
  /* quit if we ever come back */
  P1_Quit(rc);
} /* End of launch */

/*Join: Causes current Process to wait for first child to quit*/
int P1_Join(int *status){
  Check_Your_Privilege();
  if(procTable[currPid].numChildren == 0){
   return -1;
  }
  int i;
  /*get the PID of the child that quit*/ 
  // P1_P(Join_Semaphore);
  for( i= 0; i < P1_MAXPROC; i++){
    if(procTable[i].parent==currPid&&procTable[i].state==3){
      *status=procTable[i].status;
      procTable[currPid].numChildren--;
      removeProc(i);
      return i;
    }
  }
  return -2;
}

/* ------------------------------------------------------------------------
   Name - P1_Quit
   Purpose - Causes the process to quit and wait for its parent to call P1_Join.
   Parameters - quit status
   Returns - nothing
   Side Effects - the currently running process quits
   ------------------------------------------------------------------------ */
void P1_Quit(int status) {
  Check_Your_Privilege();
//  USLOSS_Console("In quit PID -- before:  %d\n", currPid);
  
  int i;
  for (i = 0; i < P1_MAXPROC; i++) {
      if(procTable[i].parent==currPid){
          procTable[i].isOrphan = 1; // orphanize the children
      }
  }

 // USLOSS_Console("Process: %s Quitting\n",procTable[currPid].name);
  
  procTable[currPid].state = 3;
  procTable[currPid].status = status;
  numProcs--;
 // USLOSS_Console("Process state: %d\n", procTable[currPid].state);
 // USLOSS_Console("PID :  %d\n", currPid);
  
  /*Remove from Ready List*/
  removeFromList(currPid);
  /*Add to blocked List*/
  addToQuitList(currPid);
  // printList(&quitListHead);
  // addToBlockedList(currPid);
  
  /*remove if orphan*/
  if(procTable[currPid].isOrphan){//||procTable[currPid].parent==-1){
      removeProc(currPid);
  }

  //USLOSS_Console("In quit PID -- after:  %d\n", currPid);
  dispatcher();
}
/*End of Quit*/

/*Removes a pcb from procTable*/
void removeProc(int PID){
    // USLOSS_Console("Removing PCB %d\n",PID); 
    if(procTable[PID].priority == -1){
      return;
    }  
    procTable[PID].priority = -1;
    /*Remove from quit list*/
    removeFromList(PID);
}

int P1_Kill(int PID){//Remove 2nd Parameter
  Check_Your_Privilege();
  if(PID==currPid){
    return -2;
  }if(PID < 0 ||PID >= P1_MAXPROC){
    return -1;
  }
  procTable[PID].state=2;
  return 0;
}

/* ------------------------------------------------------------------------
   Name - sentinel
   Purpose - The purpose of the sentinel routine is two-fold.  One
             responsibility is to keep the system going when all other
             processes are blocked.  The other is to detect and report
             simple deadlock states.
   Parameters - none
   Returns - nothing
   Side Effects -  if system is in deadlock, print appropriate error
                   and halt.
   ----------------------------------------------------------------------- */
int sentinel (void *notused)
{
 // USLOSS_Console("in sentinel, NumProcs=%d\n",numProcs);
 // printList(semTable[0].queue);
  /*No Interupts within Part 1 so commented out*/
  while (numProcs > 1)
  {
    //Check for deadlock here 
    USLOSS_WaitInt();
  }
  USLOSS_Halt(0);
  /* Never gets here. */
  return 0;
} /* End of sentinel */