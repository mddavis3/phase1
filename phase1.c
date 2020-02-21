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
void dump_processes();
static void insertRL(proc_ptr);
void printReadyList();
int zap(int);
int is_zapped(void);
static void removeFromRL(int);
void insertChild(proc_ptr);
int check_io(void);
int getpid(void);
int block_me(int);
int unblock_proc(int);
int read_cur_start_time(void);
void time_slice(void);
int readtime(void);
void insertZapList(proc_ptr);
void dezappify(void);
void clock_handler(int, void *);




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
proc_struct DummyStruct = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0};

/* define the variable for the interrupt vector declared by USLOSS */
void(*int_vec[NUM_INTS])(int dev, void * unit);



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
   int_vec[CLOCK_DEV] = clock_handler;
   

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

   dump_processes();
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
      Current->num_kids ++;   //add a kid to the num_kids slot
   }

   /* Point to process in the ReadyList */
   insertRL(&ProcTable[proc_slot]);

   //debug stuff!!!! delete later maybe!!! print readylist!!!
   //printReadyList();
   //console("\n");

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

   //Process is zapped while waiting for child to quit
   if (Current->is_zapped == ZAPPED)
   {
      return -1;
   }

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
   }// end child check code


   //set status to QUIT
   console("quit(): change status to QUIT\n");
   Current->status = QUIT;
   console("quit(): status of %s is %d (3 == QUIT)\n", Current->name, Current->status);

   //unblock processes that zapped this process
   dezappify();

   //unblock parent who called join
   if(Current->parent_ptr != NULL && Current->parent_ptr->status == BLOCKED)
   {
      console("quit(): unblock %s who called join, insert to ready list\n", Current->parent_ptr->name);
      Current->parent_ptr->status = READY;
      insertRL(Current->parent_ptr);
      //printReadyList();
      Current->parent_ptr->num_kids --;   //Decrement kid counter of parent process
      console("\n");
   }

   //send quit code to the parent process exit_code PCB entry
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
   if(Current != NULL && Current->priority <= ReadyList->priority && Current->status == RUNNING && readtime() < 80)//if true skip context switch
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
      next_process->start_time = sys_clock(); //sets start_time in microseconds
      context_switch(NULL, &next_process->state);
   }
   else if (old_process->status == QUIT) //if old_process has quit
   {
      next_process->status = RUNNING;
      removeFromRL(next_process->pid);
      console("dispatcher(): context_switch from %s to %s\n", old_process->name, next_process->name);
      //printReadyList();
      console("\n");
      old_process->pc_time = old_process->pc_time + readtime(); //get time spent in porcessor for old_process and update pc_time
      next_process->start_time = sys_clock(); //sets start_time in microseconds
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
         //printReadyList();
         console("\n");
      }

      console("dispatcher(): context_switch from %s to %s\n", old_process->name, next_process->name);
      old_process->pc_time = old_process->pc_time + readtime(); //get time spent in porcessor for old_process and update pc_time
      next_process->start_time = sys_clock(); //sets start_time in microseconds
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


/* -----------------------------------------------------------------------
   Name - check deadlock
   Purpose - check to determine if deadlock has occurred...
   Parameters - None
   Returns -  Nothing
   --------------------------------------------------------------------- */
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
   Name - disableInterrupts()
   Purpose - Disables the interrupts.
   Parameters - None
   Returns - Nothing
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
   Name - enableInterrupts()
   Purpose - Enables the interrupts.
   Parameters - None
   Returns - Nothing
   ---------------------------------------------------------------------------------*/
static void enableInterrupts()
{
   psr_set((psr_get() | PSR_CURRENT_INT));
}  /*enableInterrupts*/


/* ---------------------------------------------------------------------------------
   clock_handler()
   will call timeslice() to check if current process has exceeded its time slice
   if it has exceeded time slice, timeslice() calls dispatcher   
}
   ---------------------------------------------------------------------------------*/
void clock_handler(int dev, void *unit)
{
   time_slice();
   return;
} /* clock_handler */


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
   int proc_slot = 0;

   while (ProcTable[proc_slot].pid != pid)
   {
      proc_slot++;
      if (proc_slot == MAXPROC)
      {
         console("zap(): Tried to zap a process that does not exist\n");
         halt(1);
      }
   }

   if (ProcTable[proc_slot].pid == Current->pid)
   {
      console("zap(): Process tried to zap itself. Error.\n");
      halt(1);
   }

   //process to be zapped has been found and is_zapped is set to Zapped
   ProcTable[proc_slot].is_zapped == ZAPPED;

   //Make the linked list of zappers to the zappee
   insertZapList(&ProcTable[proc_slot]);
   
   //This process called zap and now needs to be blocked
   Current->status == BLOCKED;

   //current process blocked, dispatcher needs to be called
   console("zap(): calling dispatcher\n");
   dispatcher();

   //if the process was zapped while in zap function, return -1
   if (Current->is_zapped == ZAPPED)
   {
      return -1;
   }
   //if the zapped process has called quit, return 0
   if (ProcTable[proc_slot].status == QUIT)
   {
      return 0;
   }

   //redundant just in case
   return 0;
}/* zap */


/* ---------------------------------------------------------------------------------
   Name - is_zapped
   Purpose - Checks if the current process is_Zapped or not
   Parameters - none
   Returns -  Nothing
   Side effects - 
   ---------------------------------------------------------------------------------*/
int is_zapped(void)
{

   if(Current->is_zapped == ZAPPED)
   {
      return ZAPPED;
   }
   else
   {
   return NOT_ZAPPED;   // if is_zapped element in PCB struct is set to null the else statment should force NOT_ZAPPED to be returned
   }
}/* is_zapped */



/* ------------------------------------------------------------------------------
   Name - dump_processes()
   Purpose - prints information about the entries in the Proc Table
   Parameters - None
   Returns - Nothing
   ------------------------------------------------------------------------------*/
void dump_processes()
{
   int i = 0;
   console("\n==============================dump_processes====================================\n");
   while (ProcTable[i].pid != NULL && i < MAXPROC)
   {
      console("ProcTable entry %d:\n", i);
      console("Name: %s\n", ProcTable[i].name);
      console("PID: %d\n", ProcTable[i].pid);
      //console("Parent PID: %d\n", ProcTable[i].parent_ptr->pid);
      if(ProcTable[i].parent_ptr == NULL)
      {
         console("Parent PID: N/A\n");
      }
      else
      {
         console("Parent PID: %d\n", ProcTable[i].parent_ptr->pid);
      }

      if(ProcTable[i].status == READY)
      {
      console("Status: READY\n");
      }
      else if (ProcTable[i].status == BLOCKED)
      {
        console("Status: BLOCKED\n"); 
      }
      else console("Status: QUIT\n");

      console("# of Children: %d\n", ProcTable[i].num_kids);
      console("CPU time: %d ms\n", ProcTable[i].start_time); 
      console("-------------------\n");
      i++;
   }
   return;
}/* dump_processes */


/* -------------------------------------------------------------------------------
   Name - insertRL() 
   Purpose - inserts entries into the ReadyList in appropriate order by priority
   Parameters - a process pointer to a PCB block
   Returns -  Nothing
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
   Name - removeFromRL()
   Purpose - removes entry from the ReadyList
   Parameters - Accepts a PID of the process to be removed
   Returns -  Nothing
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
} /* removeFromRL */



/* --------------------------------------------------------------------------------
   Name - printReadyList()
   Purpose - prints the Ready List contents for debug purposes
   Parameters - None
   Returns - Nothing
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
} /*  printReadyList */


/* ------------------------------------------------------------------------------------
   insertChild()
   inserts a child process into the list of parents/children/siblings
   maintains pointers and relationships
   Parameters - Accepts a process pointer of the child to be inserted into the linked
                list of children belonging to the parent
   Returns -  Nothing
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
} /* insertChild */



/* -------------------------------------------------------------------------------
   Name - check_io()
   Purpose - Checks for input and output
            Dummy function that returns 0 in phase1
   Parameters - None
   Returns -  Nothing
   -------------------------------------------------------------------------------*/
int check_io(void)
{
   return 0;
} /* check_io */


/* -------------------------------------------------------------------------------
   Name - getpid()
   Purpose - returns the pid of current process
   Parameters - None
   Returns -  The PID of the current running Process
   -------------------------------------------------------------------------------*/
int getpid(void)
{
   return Current->pid;
} /* getpid */



/* -------------------------------------------------------------------------------
   Name - block_me()
   Purpose - blocks the calling process
             new_status is value used to indicate status of the process in the dump_processes command
             new_status must be larger than 10; if not, USLOSS will halt with error message
   Parameters - accepts the new-status value???
   Returns -   
               0 if block is successfull
              -1 if current process is zapped while blocked
   -------------------------------------------------------------------------------*/
int block_me(int new_status)
{
   if (new_status <= 10)
   {
      console("block_me: Error - new_status <= 10. Halt(1)\n");
      halt(1);
   }

   if (Current->is_zapped == ZAPPED)
   {
      return -1;
   }

   Current->status = BLOCKED;
   Current->blocked_status = new_status;
   return 0;
}

/* -------------------------------------------------------------------------------
   Name - unblock_proc()
   Purpose - unblocks process with pid that had been blocked by calling block_me
             status is changed to READY and put into ReadyList
             dispatcher will be called as a side-effect
   Parameters - accepts pid of process to be unblocked
   Returns - 
               -2 if the indicated process was not blocked, does not exist, is the Current process,
                  or is blocked on a status less than or equal to 10.

               -1 if the calling process was zapped

               0 if unblock is sucessful
   -------------------------------------------------------------------------------*/
int unblock_proc(int pid)
{
   if (Current->is_zapped == ZAPPED)
   {
      return -1;
   }

   int i = 0;
   while (ProcTable[i].pid != pid && i < MAXPROC)
   {
      i++;
   }
   if (i == MAXPROC)
   {
      return -2;
   }
   if (ProcTable[i].status != BLOCKED)
   {
      return -2;
   }
   if (ProcTable[i].pid == Current->pid)
   {
      return -2;
   }
   if (ProcTable[i].blocked_status <= 10)
   {
      return -2;
   }

   ProcTable[i].status = READY;
   insertRL(&ProcTable[i]);
   dispatcher();
   return 0;
} /* unblock_proc */



/* -------------------------------------------------------------------------------
   Name - read_cur_start_time()
   Purpose - returns the time (in microseconds) at which the current process 
             began its current time slice
   Parameters - None
   Returns -  A time stamp of when process started its current time slice
   -------------------------------------------------------------------------------*/
int read_cur_start_time(void)
{
   return (Current->start_time);
} /* read_cur_start_time */



/* -------------------------------------------------------------------------------
   Name - time_slice()
   Purpose - calls the dispatcher if currently executing process has exceeded its time slice
             otherwise it simply returns
   Parameters - None
   Returns - Nothing
   -------------------------------------------------------------------------------*/
void time_slice(void)
{
   if (readtime() >= 80)
   {
      dispatcher();
   }
   return;
} /* time_slice */



/* -------------------------------------------------------------------------------
   Name - readtime()
   Purpose - returns CPU time (in milliseconds) used by the current process.
   Parameters - None
   Returns - CPU time used by current process
   -------------------------------------------------------------------------------*/
int readtime(void)
{
   int time;
   time = (sys_clock() - read_cur_start_time()) / 1000; // 1000 microseconds = 1 millisecond
   return time;
} /* read time */



/* --------------------------------------------------------------------------------
   insertZapList()
   configures correct pointers to all zappers
   walks through next_zapper_ptr list
   --------------------------------------------------------------------------------*/
void insertZapList(proc_ptr zappee)
{
   proc_ptr walker;

   if(zappee->zapped_by_ptr == NULL)
   {
      zappee->zapped_by_ptr = Current;
   }
   else
   {
      walker = zappee->zapped_by_ptr;
      while(walker->next_zapper_ptr != NULL)
      {
         walker = walker->next_zapper_ptr;
      }
      walker->next_zapper_ptr = Current;
   }
   
   return;
} /* insertZapList */


/* --------------------------------------------------------------------------
   dezappify()
   unblocks all of the zappers that zapped this process
   --------------------------------------------------------------------------*/
void dezappify(void)
{
   proc_ptr walker;
   proc_ptr previous;
   
   if (Current->zapped_by_ptr == NULL)
   {
      return;
   }
   else
   {
      walker = Current->zapped_by_ptr;
      Current->zapped_by_ptr = NULL;

      while (walker->next_zapper_ptr != NULL)
      {
         //align pointers to their respective PCBs
         previous = walker;
         walker = walker->next_zapper_ptr;

         //clean-up previous block in list
         previous->next_zapper_ptr = NULL;
         previous->status = READY;
         insertRL(previous);
      }

      //clean-up walker block at the end of list
      walker->status = READY;
      insertRL(walker);

      return;
   }
} /* dezappify */