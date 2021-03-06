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
/*Define Static Semaphores*/
#define Fork_Sema 0
#define Clock_Sema 50
#define Alarm_Sema 51
#define Term_Sema 52
#define MMU_Sema 53
#define Sys_Sema 54
#define Disk_Sema 55


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
    int waitingOnDevice;
    struct PCB* nextPCB;
    struct PCB* prevPCB;    
} PCB;

typedef struct 
{
  int value;
  int valid;
  struct PCB *queue;
}Semaphore;

int timeTracker=0;
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
void addToProcQue(int PID, Semaphore sem);
void int_disable();
void int_enable();
void dispatcher();


int P1_WaitDevice(int type, int unit, int *status){
  if(procTable[currPid].state==2){//Checks if killed
    return -3;
  }
  /*TO DO: Check If valid Unit*/
  if(unit<0||unit>3){
    return -1;
  }
  Semaphore* sema;
  switch(type){
    case USLOSS_CLOCK_INT:
      if(unit!=1){
        return -1;
      }
      USLOSS_WaitInt();
      *status=-1;
      break;
    case USLOSS_ALARM_INT:
      if(unit!=1){
        return -1;
      }
      sema=&semTable[Alarm_Sema];
      P1_P(sema);
      *status=-1;
      break;
    case USLOSS_DISK_INT:
      if(unit<1||unit>2){
        return -1;
      }
      sema=&semTable[Disk_Sema];
      P1_P(sema);
      *status=-1;
      break;
    case USLOSS_TERM_INT:
      if(unit<1||unit>4){
        return -1;
      }
      sema=&semTable[Term_Sema];
      P1_P(sema);
      *status=-1;
      break;
    default:
      return -2;//Invalid Type
  }
  return 0;
}
void clockHandler(){
  int lastTime = USLOSS_Clock();
  ++timeTracker;
  if(USLOSS_Clock()-lastTime >= 5){
    // USLOSS_Console("Clock Fun at %d\n",USLOSS_Clock());
    // clock semaphore
    timeTracker = 0;
    // clock semaphore!!!!!!
    P1_V(&semTable[Clock_Sema]);
    dispatcher();
  }
  
}

void tempAlarmHandler(){
  USLOSS_Console("Alarm handler not implemented");
  P1_Quit(2);
}

void tempDiskHandler(){
  USLOSS_Console("Disk handler not implemented");
  P1_Quit(3);
}
void tempTermHandler(){
  USLOSS_Console("Term handler not implemented");
  P1_Quit(4);
}
void tempMMUHandler(){
  USLOSS_Console("MMU handler not implemented");
  P1_Quit(5);
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
    // USLOSS_Console("empty list\n");
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


void removeFromList(int PID){
  if(procTable[PID].nextPCB!=NULL){
    procTable[PID].nextPCB->prevPCB=procTable[PID].prevPCB;  
  }
  procTable[PID].prevPCB->nextPCB=procTable[PID].nextPCB;
  procTable[PID].nextPCB=NULL;
  procTable[PID].prevPCB=NULL;
}
void addToProcQue(int PID, Semaphore sem){
  
  PCB* pos=sem.queue;
  while(pos->nextPCB!=NULL){
    pos=pos->nextPCB;
  }
  
  pos->nextPCB=&procTable[PID];
  procTable[PID].nextPCB=NULL;
  procTable[PID].prevPCB=pos;
  
}

/* -------------------------- Functions ----------------------------------- */
/* ------------------------------------------------------------------------
   Name - dispatcher
   Purpose - runs the highest priority runnable process
   Parameters - none
   Returns - nothing
   Side Effects - runs a process
   ----------------------------------------------------------------------- */
void dispatcher()
{
  Check_Your_Privilege();

  int_disable();
  // USLOSS_Console("dispatcher Called, ready List: ");
  // printList(&readyHead);
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

// USLOSS_Console("dispatcher Check point 1\n");
  readyListPos->nextPCB->state=0;//set state to running
  /*Set Proc state to ready unless it has quit or been killed*/
  if(procTable[oldpid].state==0){
    procTable[oldpid].state=1;
  }
  // USLOSS_Console("dispatcher Check point 2\n");
  currPid = readyListPos->nextPCB->PID;
  procTable[currPid].lastStartedTime=USLOSS_Clock();
  // USLOSS_Console("dispatcher Check point 3\n");
  /*Adds currpid to end of its priority section in the Rdy list*/
  removeFromList(currPid);
  addToReadyList(currPid);
  /*Reenable Interrupts*/
  int_enable();
  // USLOSS_Console("In dispatcher switching to PID:  %d\n", currPid); 
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
  // int_disable();
  int i;
  /* initialize the process table here */
  for(i = 0; i < P1_MAXPROC; i++){
      PCB dummy;
      procTable[i]=dummy;
      //USLOSS_Context DummyCon;
      procTable[i].priority = -1;
      //procTable[i].context=DummyCon;
  }

  /* Initialize the Ready list, Blocked list, etc. here */
  readyHead.prevPCB=NULL;
  readyHead.nextPCB=NULL;
  blockedHead.prevPCB=NULL;
  blockedHead.nextPCB=NULL;
  quitListHead.nextPCB=NULL;
  quitListHead.prevPCB=NULL;
  /* Initialize the interrupt vector here */
  USLOSS_IntVec[USLOSS_CLOCK_INT] = &clockHandler;
  USLOSS_IntVec[USLOSS_ALARM_INT] = &tempAlarmHandler;
  USLOSS_IntVec[USLOSS_DISK_INT] = &tempDiskHandler;
  USLOSS_IntVec[USLOSS_TERM_INT] = &tempTermHandler;
  USLOSS_IntVec[USLOSS_MMU_INT] = &tempMMUHandler;
  USLOSS_IntVec[USLOSS_SYSCALL_INT] = &tempSyscallHandler;
  /* Initialize the semaphores here */
  

/*initialize the semaphore table*/
  for(i=0; i< P1_MAXSEM; i++){
    Semaphore *dummy = malloc(sizeof(Semaphore));
    PCB listHead;
    dummy->queue=&listHead;
    semTable[i] = *dummy;
    semTable[i].value = -1;
    semTable[i].valid = 0;
    semTable[i].queue = malloc(sizeof(PCB));
  }

  /*Fork Semaphore*/
  P1_SemCreate(1);
  /* semaphores for the processes */
  for(i=1; i < 50; i++){
    P1_SemCreate(0);
  }
  /* semaphores for the 6 devices */
  for(i=0; i < 6; i++){
    P1_SemCreate(0);
  }

 
  /* startup a sentinel process */
  /* HINT: you don't want any forked processes to run until startup is finished.
   * You'll need to do something in your dispatcher to prevent that from happening.
   * Otherwise your sentinel will start running right now and it will call P1_Halt. 
   */
  P1_Fork("sentinel", sentinel, NULL, USLOSS_MIN_STACK, 6);

  /* start the P2_Startup process */
  P1_Fork("P2_Startup", P2_Startup, NULL, 4 * USLOSS_MIN_STACK, 1);

  // int_enable();
  // USLOSS_Console("Startup finished\n");
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
    USLOSS_Console("Error: Access Denied to User Mode\n");
    exit(1);
  }
}

void int_disable(){
  USLOSS_PsrSet(USLOSS_PsrGet() & ~(USLOSS_PSR_CURRENT_INT));
}
void int_enable(){
  USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);
}

P1_Semaphore P1_SemCreate(unsigned int value){
  int_disable();
  P1_Semaphore semPointer; 
  //Semaphore *semi = malloc(sizeof(Semaphore));

  // put the semaphore in the table
  int i = 0;
  while(semTable[i].value != -1 && i < P1_MAXSEM){
    i++;
  }
  semTable[i].value = value;
  semTable[i].valid = i;
  semTable[i].queue->nextPCB = NULL;
  semTable[i].queue->prevPCB = NULL;
  semPointer = &semTable[i];

  int_enable();
  return semPointer;
}

int P1_SemFree(P1_Semaphore sem){
  Check_Your_Privilege();
  //if sem is invalid return -1, sem is invalid if it is not created using SemCreate method
  Semaphore* semP=(Semaphore*)sem;
  if(semP->value < 0 || semP->valid==0){
    USLOSS_Console("Semaphore is invalid\n");
    USLOSS_Halt(1);
    return -1;
  }
  semP->value = -1;
  free(sem);
  return 0;
}

int P1_P(P1_Semaphore sem){
  Check_Your_Privilege();
  Semaphore* semP=(Semaphore*)sem;
  if(semP->valid>Clock_Sema&&semP->valid<MMU_Sema){
    // USLOSS_Console("In P1_P for %d,Sem Table Pos: %d\n",currPid,semP->valid);
    procTable[currPid].waitingOnDevice=1;
  }
  // check if the process is killed
  if(procTable[currPid].state == 2){
    return -2;
  }
  // check if the semaphore is valid
  if(semP==NULL||semP->valid<0){
    USLOSS_Console("Semaphore is invalid\n");
    return - 1;
  }
  // move process from ready queueu to semaphore->procQue
  
  // USLOSS_Console("In P1_P for %d\n",currPid);
  while(1){
    int_disable();
    if(semP->value > 0){
      semP->value--;
      break;
    }
    if(currPid!=-1){
      procTable[currPid].state = 4; // waiting status
      // USLOSS_Console("CurrPid %d has state=%d\n",currPid,procTable[currPid].state);
      removeFromList(currPid);
      addToProcQue(currPid,*semP);
      dispatcher();
    }
    int_enable();
    // USLOSS_Console("In P1_P for %d\n",currPid);

  }

  int_enable();
  // USLOSS_Console("Exiting P1_P for %d\n",currPid);
  //interrupt enable
  return 0;
}

int P1_V(P1_Semaphore sem){
  // USLOSS_Console("In P1_V for %s\n",procTable[currPid].name);
  Check_Your_Privilege();
  // interrupt disable HERE!
  int_disable();
  Semaphore* semP=(Semaphore*)sem;
  
// check if the semaphore is valid
  if(semP==NULL||semP->valid<0){
    USLOSS_Console("Semaphore is invalid\n");
    return - 1;
  }
  semP->value++;
  if(currPid!=-1&&semP->queue->nextPCB != NULL){
    // USLOSS_Console("In P1_V for %s\n",procTable[currPid].name);
    int PID=semP->queue->nextPCB->PID;
    // USLOSS_Console("P1_V Pid: %d",PID);
    //Move first frocess from procQueue to ready queue
    // if(procTable[PID].state == 4){
      procTable[PID].state = 1; // ready status
      procTable[currPid].waitingOnDevice=0;
      //removeToProcQue(currPid,*semP);
      removeFromList(PID);
      addToReadyList(PID);
      // USLOSS_Console("ReadyList after P1_V for %s: ",procTable[PID].name);
      // printList(&readyHead);

      dispatcher();
    // }
  }
  int_enable();
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
    // USLOSS_Console("Proc %s passed Privilege Check\n",name);
    //free the available spots
    free_Procs();

    /*Check Priority and Stack Size*/
    if(priority<1||priority>6){//is priority # valid
      return -3;
    }
    if(stacksize<USLOSS_MIN_STACK){//is stacksize valid
      return -2;
    }
    // P1_Semaphore sema=&semTable[0];
    //find PID
    int newPid = 0;
    while(procTable[newPid].priority!=-1){
      newPid++;
      if(newPid>=P1_MAXPROC){
        return -1;
      }
    }

    int_disable();
    /* stack = allocated stack here */
    // void* newStack=malloc(stacksize*sizeof(char));
    procTable[newPid].stack=malloc(stacksize*sizeof(char));
    procTable[newPid].notFreed=1;

    /*set PCB fields*/
    procTable[newPid].PID=newPid;
    procTable[newPid].cpuTime=0;
    procTable[newPid].lastStartedTime=FIRST_RUN;
    procTable[newPid].state=1;//0=running 1=ready,2=killed,3=quit,4=waiting

    procTable[newPid].status=DEFAULT;
    procTable[newPid].isOrphan= 0;
    if(currPid==-1){
      // USLOSS_Console("Start up %s Orphan\n",name);
      procTable[newPid].isOrphan=1;
    }   
    
    procTable[newPid].parent=currPid;
    procTable[currPid].numChildren++;//increment parents numChildren
    procTable[newPid].numChildren=0;
    procTable[newPid].priority=priority;
    procTable[newPid].waitingOnDevice=0;
    procTable[newPid].name=strdup(name);
    procTable[newPid].startFunc = f;
    procTable[newPid].startArg = arg;
    
    /*PCB Fields are set*/

    // USLOSS_Console("proc %s forked to PID %d\n",name,newPid);
    /*add to ready list*/
    addToReadyList(newPid);    

    /*increment numProcs*/
    numProcs++;
    
    /*initialize context*/
    USLOSS_ContextInit(&(procTable[newPid].context), USLOSS_PsrGet(), procTable[newPid].stack, 
        stacksize, launch);
    int_enable();

    
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
  P1_Semaphore sema = &semTable[currPid];
  // USLOSS_Console("Join:Before P\n");
  P1_P(sema);
  // USLOSS_Console("Join:After P\n");
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

 // USLOSS_Console("Process: %s Quitting pid=%d\n",procTable[currPid].name,currPid);
  
  procTable[currPid].state = 3;
  procTable[currPid].status = status;
  numProcs--;
 // USLOSS_Console("Process state: %d\n", procTable[currPid].state);
 // USLOSS_Console("PID :  %d\n", currPid);
  
  /*Remove from Ready List*/
  removeFromList(currPid);
  /*Add to blocked List*/
  addToQuitList(currPid);

  // USLOSS_Console("Orphan Status: %d",procTable[currPid].isOrphan);
  /*remove if orphan*/
  if(procTable[currPid].isOrphan){//||procTable[currPid].parent==-1){
      removeProc(currPid);
  }else{
    P1_Semaphore semi = &semTable[procTable[currPid].parent];
    // USLOSS_Console("Quit P1_V for %d",currPid);
    P1_V(semi);
  }

  //USLOSS_Console("In quit PID -- after:  %d\n", currPid);
  
  // USLOSS_Console("Number of processes: %d\n", numProcs);
  dispatcher();
}
/*End of Quit*/

/*Removes a pcb from procTable*/
void removeProc(int PID){
    //USLOSS_Console("Removing PCB %d\n",PID); 
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
  /*No Interupts within Part 1 so commented out*/
  // int deadlock=1;
  int i;
  // printList(&readyHead);
  while (numProcs > 1){
    USLOSS_Console("in sentinel\n");
    //Check for deadlock here 
    for(i=1;i<P1_MAXPROC;i++){
      if(procTable[i].state == 4&&procTable[i].waitingOnDevice==0){
        USLOSS_Console("Error: Deadlock\n");
        USLOSS_Halt(1);
      }
    }
    USLOSS_WaitInt();
  }
  USLOSS_Halt(0);
  /* Never gets here. */
  return 0;
} /* End of sentinel */