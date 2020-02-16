/* ------------------------------------------------------------------------
   phase1.c

   CSCV 452

   ------------------------------------------------------------------------ */
#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <phase1.h>
#include "kernel.h"
#include <string.h>

/* ------------------------- Prototypes ----------------------------------- */
int sentinel (void *);
extern int start1 (char *);
void dispatcher(void);
void launch();
static void enableInterrupts();
static void check_deadlock();
void printProcTable();
static void insertRL(proc_ptr);
void printReadyList();
int zap(int);
int is_zapped(void);
static void removeFromRL(int);
void insertChild(proc_ptr);
int check_io(void);



/* -------------------------- Globals ------------------------------------- */

/* Patrick's debugging global variable... */
int debugflag = 1;

/* the process table */
proc_struct ProcTable[MAXPROC];

/* Process lists  */
/* ReadyList is a linked list of process pointers */
proc_ptr ReadyList = NULL;

/* current process ID */
proc_ptr Current;

/* the next pid to be assigned */
unsigned int next_pid = SENTINELPID;

/* empty proc_struct */
proc_struct DummyStruct = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

/* define the variable for the interrupt vector declared by USLOSS */
//void(*int_vec[NUM_INTS])(int dev, void * unit);



/* -------------------------- Functions ----------------------------------- */
/* ------------------------------------------------------------------------
   Name - startup
   Purpose - Initializes process lists and clock interrupt vector.
	     Start up sentinel process and the test process.
   Parameters - none, called by USLOSS
   Returns - nothing
   Side Effects - lots, starts the whole thing
   ----------------------------------------------------------------------- */
void startup()
{
   int i;      /* loop index */
   int result; /* value returned by call to fork1() */

   /* initialize the process table */
   for( i = 0; i < MAXPROC; i++)
   {
      ProcTable[i] = DummyStruct;
   }

   /* Initialize the Ready list, etc. */
   if (DEBUG && debugflag)
      console("startup(): initializing the Ready & Blocked lists\n");
   

   /* Initialize the clock interrupt handler */
   //int_vec[CLOCK_DEV] = clock_handler;
   

   /* startup a sentinel process */
   if (DEBUG && debugflag)
       console("startup(): calling fork1() for sentinel\n");
   result = fork1("sentinel", sentinel, NULL, USLOSS_MIN_STACK, SENTINELPRIORITY);
   if (result < 0) {
      if (DEBUG && debugflag)
         console("startup(): fork1 of sentinel returned error, halting...\nResult = %d\n", result);
      halt(1);
   }

   /* start the test process */
   if (DEBUG && debugflag)
      console("startup(): calling fork1() for start1\n");
   result = fork1("start1", start1, NULL, 2 * USLOSS_MIN_STACK, 1);
   if (result < 0) {
      console("startup(): fork1 for start1 returned an error, halting...\n");
      halt(1);
   }

   printProcTable();
   console("startup(): Should not see this message! ");
   console("Returned from fork1 call that created start1\n");

   return;
} /* startup */

/* ------------------------------------------------------------------------
   Name - finish
   Purpose - Required by USLOSS
   Parameters - none
   Returns - nothing
   Side Effects - none
   ----------------------------------------------------------------------- */
void finish()
{
   if (DEBUG && debugflag)
      console("in finish...\n");
} /* finish */

/* ------------------------------------------------------------------------
   Name - fork1
   Purpose - Gets a new process from the process table and initializes
             information of the process.  Updates information in the
             parent process to reflect this child process creation.
   Parameters - the process procedure address, the size of the stack and
                the priority to be assigned to the child process.
   Returns - the process id of the created child or -1 if no child could
             be created or if priority is not between max and min priority.
   Side Effects - ReadyList is changed, ProcTable is changed, Current
                  process information changed
   ------------------------------------------------------------------------ */
//fork1(char *name, int (*f)(void *), void *arg, int stacksize, int priority)
//Above is the original skeleton.c function definition
//Below is a modified definition included in phase1.h in usloss/build/include   
int fork1(char *name, int(*f)(char *), char *arg, int stacksize, int priority)
{
   int proc_slot;

   if (DEBUG && debugflag)
      console("fork1(): creating process %s\n", name);

   /* test if in kernel mode; halt if in user mode */
   if ((PSR_CURRENT_MODE & psr_get()) == 0)
   {
      halt(1);
   }

   /* Return if stack size is too small */
   if (stacksize < USLOSS_MIN_STACK)
   {
      return (-2);
   }

   /* create a stack pointer, malloc stack using the stacksize */
   char* stackPtr = (char*) malloc (stacksize * sizeof(int));


   /* find an empty slot in the process table */
   proc_slot = 0;
   while (ProcTable[proc_slot].pid != NULL)
   {
      proc_slot++;
      if (proc_slot == MAXPROC)
      {
         return -1;
      }
   }

   /* fill-in entry in process table */
   /* process name */
   if ( strlen(name) >= (MAXNAME - 1) ) {
      console("fork1(): Process name is too long.  Halting...\n");
      halt(1);
   }
   strcpy(ProcTable[proc_slot].name, name);

   /* process starting function */
   ProcTable[proc_slot].start_func = f;

   /* process function argument */
   if ( arg == NULL )
      ProcTable[proc_slot].start_arg[0] = '\0';
   else if ( strlen(arg) >= (MAXARG - 1) ) {
      console("fork1(): argument too long.  Halting...\n");
      halt(1);
   }
   else
      strcpy(ProcTable[proc_slot].start_arg, arg);
   
   /* process stack pointer */
   ProcTable[proc_slot].stack = stackPtr;

   /* process stacksize */
   ProcTable[proc_slot].stacksize = stacksize;

   /* process pid */
   ProcTable[proc_slot].pid = next_pid++;

   /* process priority */
   ProcTable[proc_slot].priority = priority;

   /* process status (READY by default) */
   ProcTable[proc_slot].status = READY;

   /* if Current is a Parent process, create the link to the child using insertChild method */
   if (Current != NULL)
   {
      insertChild(&ProcTable[proc_slot]); 
   }

   /* Point to process in the ReadyList */
   insertRL(&ProcTable[proc_slot]);

   //debug stuff!!!! delete later maybe!!! print readylist!!!
   printReadyList();
   console("\n");

   /* Initialize context for this process, but use launch function pointer for
    * the initial value of the process's program counter (PC)
    */
   context_init(&(ProcTable[proc_slot].state), psr_get(),
                ProcTable[proc_slot].stack, 
                ProcTable[proc_slot].stacksize, launch);

   /* call dispatcher - exception for sentinel */
   if (strcmp(ProcTable[proc_slot].name, "sentinel") != 0)
   {
      console("fork1(): calling dispatcher\n");
      dispatcher();
   }
   
   /* for future phase(s) */
   p1_fork(ProcTable[proc_slot].pid);

   /* return pid */
   return (ProcTable[proc_slot].pid);

} /* fork1 */

/* ------------------------------------------------------------------------
   Name - launch
   Purpose - Dummy function to enable interrupts and launch a given process
             upon startup.
   Parameters - none
   Returns - nothing
   Side Effects - enable interrupts
   ------------------------------------------------------------------------ */
void launch()
{
   int result;

   if (DEBUG && debugflag)
      console("launch(): started\n");

   /* Enable interrupts */
   enableInterrupts();

   /* Call the function passed to fork1, and capture its return value */
   result = Current->start_func(Current->start_arg);

   if (DEBUG && debugflag)
      console("Process %d returned to launch\n", Current->pid);

   quit(result);

} /* launch */


/* ------------------------------------------------------------------------
   Name - join
   Purpose - Wait for a child process (if one has been forked) to quit.  If 
             one has already quit, don't wait.
   Parameters - a pointer to an int where the termination code of the 
                quitting process is to be stored.
   Returns - the process id of the quitting child joined on.
		-1 if the process was zapped in the join
		-2 if the process has no children
   Side Effects - If no child process has quit before join is called, the 
                  parent is removed from the ready list and blocked.
   ------------------------------------------------------------------------ */
int join(int *code)
{
   //Process is zapped 
   if (Current->is_zapped == ZAPPED)
   {
      return -1;
   }

   //Process has no children
   if(Current->child_proc_ptr == NULL)
   {
      return -2;  
   }

   //current process has called join so needs to be blocked until child process quits 
   Current->status = BLOCKED;

   //current process blocked, dispatcher needs to be called
   console("join(): calling dispatcher\n");
   dispatcher();

   //set exit code for child of parent calling join
   *code = Current->child_proc_ptr->exit_code;

   //return the pid of the quitting child process that is joined on
   return Current->child_proc_ptr->pid;

} /* join */


/* ------------------------------------------------------------------------
   Name - quit
   Purpose - Stops the child process and notifies the parent of the death by
             putting child quit info on the parents child completion code
             list.
   Parameters - the code to return to the grieving parent
   Returns - nothing
   Side Effects - changes the parent of pid child completion status list.
   ------------------------------------------------------------------------ */
void quit(int code)
{
   //if the parent has active children, halt(1)
   console("quit(): checking for active children\n");
   if (Current->child_proc_ptr != NULL)
   {
      proc_ptr walker = Current->child_proc_ptr;
      if (walker->status != QUIT)
      {
         console("Error! Active child status = %d\n", walker->status);
         console("Error! Process has active children and cannot quit.\n");
         halt(1);
      }
      else
      {
         while (walker->next_sibling_ptr != NULL)
         {
            walker = walker->next_sibling_ptr;
            if (walker->status != QUIT)
            {
               console("Error! Active child status = %d\n", walker->status);
               console("Error! Process has active children and cannot quit.\n");
               halt(1);
            }
         }
      }  
   }

   //set status to QUIT
   console("quit(): change status to QUIT\n");
   Current->status = QUIT;
   console("quit(): status of %s is %d (3 == QUIT)\n", Current->name, Current->status);

   //cleanup PCB

   //unblock processes that zapped this process


   //unblock parent who called join
   if(Current->parent_ptr != NULL && Current->parent_ptr->status == BLOCKED)
   {
      console("quit(): unblock %s who called join, insert to ready list\n", Current->parent_ptr->name);
      Current->parent_ptr->status = READY;
      insertRL(Current->parent_ptr);
      printReadyList();
      console("\n");
   }

   //send quit code to the parent process exit_code PCB entry
   //maybe check if parent has called join???
   if(Current->parent_ptr != NULL)
   {
      console("quit(): send exit_code to parent %s\n", Current->parent_ptr->name);
      Current->parent_ptr->exit_code = code;
   }

   //call dispatcher to switch to another process
   console("quit(): call dispatcher\n");
   dispatcher();

   p1_quit(Current->pid);
} /* quit */


/* ------------------------------------------------------------------------
   Name - dispatcher
   Purpose - dispatches ready processes.  The process with the highest
             priority (the first on the ready list) is scheduled to
             run.  The old process is swapped out and the new process
             swapped in.
   Parameters - none
   Returns - nothing
   Side Effects - the context of the machine is changed
   ----------------------------------------------------------------------- */
void dispatcher(void)
{
   //if current process still has highest priority the let it run.  
   //Assuming it hasn't exceeded its time limit.  ****For now time limit is left off****
   if(Current != NULL && Current->priority <= ReadyList->priority && Current->status == RUNNING)//if true skip context switch
   {
      return;
   }

   proc_ptr next_process;
   proc_ptr old_process;

   next_process = ReadyList;
   old_process = Current;
   Current = next_process;

   if (old_process == NULL) //if starting up
   {
      next_process->status = RUNNING;
      removeFromRL(next_process->pid);
      console("dispatcher(): context_switch to %s\n", next_process->name);
      context_switch(NULL, &next_process->state);
   }
   else if (old_process->status == QUIT) //if old_process has quit
   {
      next_process->status = RUNNING;
      removeFromRL(next_process->pid);
      console("dispatcher(): context_switch from %s to %s\n", old_process->name, next_process->name);
      printReadyList();
      console("\n");
      context_switch(&old_process->state, &next_process->state);
   }
   else //if old_process is running or blocked
   {
      next_process->status = RUNNING;
      removeFromRL(next_process->pid);

      //if the "running" process is not-blocked, insert it into the ready list
      if (old_process->status != BLOCKED)
      {
         old_process->status = READY;
         insertRL(old_process);
         printReadyList();
         console("\n");
      }

      console("dispatcher(): context_switch from %s to %s\n", old_process->name, next_process->name);
      context_switch(&old_process->state, &next_process->state);
   }
   
   //p1_switch(Current->pid, next_process->pid);
} /* dispatcher */


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
int sentinel (void * dummy)
{
   if (DEBUG && debugflag)
      console("sentinel(): called\n");
   while (1)
   {
      check_deadlock();
      waitint();
   }
} /* sentinel */


/* check to determine if deadlock has occurred... */
static void check_deadlock()
{
   if (check_io() == 1)
      return;

   //run through the PCB and see if any processes are active
   for( int i = 0; i < MAXPROC; i++)
   {
      if (ProcTable[i].pid != NULL)
      {
         if (ProcTable[i].pid != SENTINELPID && ProcTable[i].status != QUIT)
         {
            console("sentinel(): active processes besides sentinel, halt(1)\n");
            console("sentinel(): status of active process %s is %d\n", ProcTable[i].name, ProcTable[i].status);
            halt(1);
         }
      }
   }

   console("sentinel(): no other active processes, halt(0)\n");
   halt(0);
} /* check_deadlock */


/* -------------------------------------------------------------------------
   disableInterrupts()
   Disables the interrupts.
   --------------------------------------------------------------------------*/
void disableInterrupts()
{
  /* turn the interrupts OFF iff we are in kernel mode */
  if((PSR_CURRENT_MODE & psr_get()) == 0) {
    //not in kernel mode
    console("Kernel Error: Not in kernel mode, may not disable interrupts\n");
    halt(1);
  } else
    /* We ARE in kernel mode */
    psr_set( psr_get() & ~PSR_CURRENT_INT );
} /* disableInterrupts */

/* --------------------------------------------------------------------------------
   enableInterrupts()
   Enables the interrupts.
   ---------------------------------------------------------------------------------*/
static void enableInterrupts()
{
   psr_set((psr_get() | PSR_CURRENT_INT));
}

/* ---------------------------------------------------------------------------------
   clock_handler function()
   *void clock_handler(int dev, void *unit){
   code inserted here
   use SYSCLOCK to check current time (perhaps required here)   
}
   ---------------------------------------------------------------------------------*/

/* ---------------------------------------------------------------------------------
   Name - zap
   Purpose - a process arranges for another process to be killed by calling zap and
             specifying the PID of the victim
   Parameters - the PID of the victim process (short pid)
   Returns - zap does not return until the victim process quits.  
             -1 if the calling process was zapped while in zap
              0 if the zapped process has called quit
   Side effects - the caller of zap is placed into Blocked status
   ---------------------------------------------------------------------------------*/
int zap(int pid)
{
   return -1;
}


/* ---------------------------------------------------------------------------------
   Name - is_zapped
   Purpose - 
   Parameters - none
   Returns - 
   Side effects - 
   ---------------------------------------------------------------------------------*/
int is_zapped(void)
{
   return 0;
}

/* ------------------------------------------------------------------------------
   printProcTable()
   prints information about the entries in the Proc Table
   ------------------------------------------------------------------------------*/
void printProcTable()
{
   int i = 0;
   while (ProcTable[i].pid != NULL && i < MAXPROC)
   {
      console("ProcTable entry %d:\nname: %s\npid: %d\nstatus: %d\n", i, ProcTable[i].name, ProcTable[i].pid, ProcTable[i].status);
      i++;
   }

   return;
}

/* -------------------------------------------------------------------------------
   insertRL() 
   inserts entries into the ReadyList
   -------------------------------------------------------------------------------*/
static void insertRL(proc_ptr proc)
{
   proc_ptr walker, previous;  //pointers to PCB
   previous = NULL;
   walker = ReadyList;
   while (walker != NULL && walker->priority <= proc->priority) {
      previous = walker;
      walker = walker->next_proc_ptr;
   }
   if (previous == NULL) {
      /* process goes at front of ReadyList */
      proc->next_proc_ptr = ReadyList;
      ReadyList = proc;
   }
   else {
      /* process goes after previous */
      previous->next_proc_ptr = proc;
      proc->next_proc_ptr = walker;
   }
   return;
} /* insertRL */

/* --------------------------------------------------------------------------------
   removeFromRL()
   removes entry from the ReadyList
   --------------------------------------------------------------------------------*/
static void removeFromRL(int PID)
{
   proc_ptr walker, tmp;
   walker = ReadyList;
   tmp = walker;

   //if sentinel is the only process in the ready list, don't do anything
   if(walker->next_proc_ptr == NULL)
   {
      return;
   }

   //if process is the first item in the ready list
   if(walker->pid == PID)
   {
      ReadyList = walker->next_proc_ptr;
      walker->next_proc_ptr = NULL;
      return;
   }

   //if process is somewhere else in the ready list, walk through the list
   while (walker != NULL && walker->pid != PID)
   {
      tmp = walker;
      walker = walker->next_proc_ptr;
   }
   if (walker->pid == PID)
   {
      tmp->next_proc_ptr = walker->next_proc_ptr;
      walker->next_proc_ptr = NULL;
      return;
   }
   return;
}



/* --------------------------------------------------------------------------------
   printReadyList()
   prints the Ready List contents for debug purposes
   --------------------------------------------------------------------------------*/
void printReadyList()
{
   proc_ptr walker;  //pointers to PCB
   walker = ReadyList;
   console("\n================================================================================\n");
   while (walker != NULL) {
      console("Process name: %s, Priority: %d-->", walker->name, walker->priority);
      walker = walker->next_proc_ptr;
   }
      console("NULL\n");
   console("================================================================================\n");
   return;
}

/* ------------------------------------------------------------------------------------
   insertChild()
   inserts a child process into the list of parents/children/siblings
   maintains pointers and relationships
   ------------------------------------------------------------------------------------*/
void insertChild(proc_ptr child)
{
   proc_ptr walker;

   if(Current->child_proc_ptr == NULL)
   {
      Current->child_proc_ptr = child;
   }
   else
   {
      walker = Current->child_proc_ptr;
      while(walker->next_sibling_ptr != NULL)
      {
         walker = walker->next_sibling_ptr;
      }
      walker->next_sibling_ptr = child;
   }
   
   child->parent_ptr = Current;
   return;
}

/* -------------------------------------------------------------------------------
   check_io()
   Checks for input and output
   Dummy function that returns 0 in phase1
   -------------------------------------------------------------------------------*/
int check_io(void)
{
   return 0;
}