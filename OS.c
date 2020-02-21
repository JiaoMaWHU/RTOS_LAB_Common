// *************os.c**************
// EE445M/EE380L.6 Labs 1, 2, 3, and 4 
// High-level OS functions
// Students will implement these functions as part of Lab
// Runs on LM4F120/TM4C123
// Jonathan W. Valvano 
// Jan 12, 2020, valvano@mail.utexas.edu


#include <stdint.h>
#include <stdio.h>
#include "../inc/tm4c123gh6pm.h"
#include "../inc/CortexM.h"
#include "../inc/PLL.h"
#include "../inc/LaunchPad.h"
#include "../inc/Timer4A.h"
#include "../inc/Timer5A.h"
#include "../inc/WTimer0A.h"
#include "../inc/LaunchPad.h"
#include "../RTOS_Labs_common/OS.h"
#include "../RTOS_Labs_common/ST7735.h"
#include "../inc/ADCT0ATrigger.h"
#include "../RTOS_Labs_common/UART0int.h"
#include "../RTOS_Labs_common/eFile.h"

// function definitions in osasm.s
void OS_DisableInterrupts(void); // Disable interrupts
void OS_EnableInterrupts(void);  // Enable interrupts
void StartOS(void);
void ContextSwitch(void);
void StartOS(void);

// Performance Measurements 
int32_t MaxJitter;             // largest time jitter between interrupts in usec
#define JITTERSIZE 64
#define NUMTHREAD 3
#define STACKSIZE 128
#define RUN 0
#define ACTIVE 1
#define SLEEP 2

uint32_t const JitterSize=JITTERSIZE;
uint32_t JitterHistogram[JITTERSIZE]={0,};
uint32_t ElapsedTimerCounter;

struct tcb{
  int32_t *sp;       // pointer to stack (valid for threads not running
	struct tcb *next;  // linked-list pointer
	struct tcb *prior;  // linked-list pointer
  uint32_t id;				 // id for the current thread
	uint32_t sleepCounter;		 // sleep state of the thread, used for suspension
	uint32_t priority;	 // the priority of the current thread (lab 3)
	uint32_t blockedState;  // current state of the thread
	uint32_t sleepState;  // current state of the thread
};

typedef struct tcb tcbType; // meaning replace "struct tcb" with tcbType
tcbType tcbs[NUMTHREAD];
tcbType *RunPt = NULL;
tcbType *TailPt = NULL;

int32_t Stacks[NUMTHREAD][STACKSIZE];
uint32_t THREAD_ID = 0;	// THREAD_ID assign to threads

tcbType* headToSleepQueue = NULL; // store the pointer for sleep tcb

void (*UserSW1Task)(void);   // user function

/*------------------------
	traverse the queue of sleeping threads
	decrease the time count
	if meets zero, insert the thread in the cycle using tailnode 
------------------------*/
void traverseSleepQueue(void){
	tcbType* tmpQueueNode = headToSleepQueue;
	uint32_t status = StartCritical(); //disable interrupt
	while(tmpQueueNode!=NULL){
		tmpQueueNode->sleepCounter -= 1;
		if(tmpQueueNode->sleepCounter==0){
			tmpQueueNode->blockedState = ACTIVE;
			tcbType* originNext = tmpQueueNode->next; // reserved for next loop
			// remove pt from queue
			if(tmpQueueNode->next==tmpQueueNode){ // only one node left
				headToSleepQueue = NULL;
			}else{
				tmpQueueNode->prior->next = tmpQueueNode->next;
				tmpQueueNode->next->prior = tmpQueueNode->prior;
			}
			// insert after tailnode
			if(TailPt==NULL){ // tailpt and runpt all null, means the only thread in OS
				RunPt=tmpQueueNode;
				TailPt=tmpQueueNode;
				TailPt->next = TailPt;
				TailPt->prior = TailPt;
			}else{
				tmpQueueNode->next = TailPt->next;
				TailPt->next->prior = tmpQueueNode;
				TailPt->next = tmpQueueNode;
				tmpQueueNode->prior = TailPt;
				TailPt = TailPt->next;
			}
			tmpQueueNode = originNext;
		}else{
			tmpQueueNode = tmpQueueNode->next;
		}
	}
	EndCritical(status);	//enable interrupt
}


/*------------------------------------------------------------------------------
  Systick Interrupt Handler
  SysTick interrupt happens every 10 ms
  used for preemptive thread switch
 *------------------------------------------------------------------------------*/
void SysTick_Handler(void) {
	traverseSleepQueue();
	ContextSwitch();
} // end SysTick_Handler

unsigned long OS_LockScheduler(void){
  // lab 4 might need this for disk formating
  return 0;// replace with solution
}
void OS_UnLockScheduler(unsigned long previous){
  // lab 4 might need this for disk formating
}


void SysTick_Init(unsigned long period){ // not set, specified in testmain1.... functions
  
}

/**
 * @details  Initialize operating system, disable interrupts until OS_Launch.
 * Initialize OS controlled I/O: serial, ADC, systick, LaunchPad I/O and timers.
 * Interrupts not yet enabled.
 * @param  none
 * @return none
 * @brief  Initialize OS
 */
void OS_Init(void){
  // put Lab 2 (and beyond) solution here
	OS_DisableInterrupts();
	PLL_Init(Bus50MHz);         // set processor clock to 50 MHz
}; 

// ******** OS_InitSemaphore ************
// initialize semaphore 
// input:  pointer to a semaphore
// output: none
void OS_InitSemaphore(Sema4Type *semaPt, int32_t value){
  // put Lab 2 (and beyond) solution here
	semaPt->Value = value;
}; 

// ******** OS_Wait ************
// decrement semaphore 
// Lab2 spinlock
// Lab3 block if less than zero
// input:  pointer to a counting semaphore
// output: none
void OS_Wait(Sema4Type *semaPt){
  // put Lab 2 (and beyond) solution here
  DisableInterrupts(); 
	while((semaPt->Value) == 0){
		EnableInterrupts();
		DisableInterrupts();
	}
	semaPt->Value -= 1;
  EnableInterrupts();
}; 

// ******** OS_Signal ************
// increment semaphore 
// Lab2 spinlock
// Lab3 wakeup blocked thread if appropriate 
// input:  pointer to a counting semaphore
// output: none
void OS_Signal(Sema4Type *semaPt){
  // put Lab 2 (and beyond) solution here
	long status;
	status = StartCritical(); 
	semaPt->Value += 1; 
	EndCritical(status);
}; 

// ******** OS_bWait ************
// Lab2 spinlock, set to 0
// Lab3 block if less than zero
// input:  pointer to a binary semaphore
// output: none
void OS_bWait(Sema4Type *semaPt){
  // put Lab 2 (and beyond) solution here
	DisableInterrupts(); 
	while((semaPt->Value) == 0){
		EnableInterrupts();
		DisableInterrupts();
	}
	semaPt->Value = 0;
  EnableInterrupts();
}; 

// ******** OS_bSignal ************
// Lab2 spinlock, set to 1
// Lab3 wakeup blocked thread if appropriate 
// input:  pointer to a binary semaphore
// output: none
void OS_bSignal(Sema4Type *semaPt){
  // put Lab 2 (and beyond) solution here
	long status;
	status = StartCritical(); 
	semaPt->Value = 1; 
	EndCritical(status);
}; 

//******** OS_SetInitialStack ***************
// Inputs: int i, which stack
// Outputs: none
// initialize a stack with special values
void OS_SetInitialStack(int i){
  tcbs[i].sp = &Stacks[i][STACKSIZE-16]; // thread stack pointer
  Stacks[i][STACKSIZE-1] = 0x01000000;   // thumb bit
  Stacks[i][STACKSIZE-3] = 0x14141414;   // R14
  Stacks[i][STACKSIZE-4] = 0x12121212;   // R12
  Stacks[i][STACKSIZE-5] = 0x03030303;   // R3
  Stacks[i][STACKSIZE-6] = 0x02020202;   // R2
  Stacks[i][STACKSIZE-7] = 0x01010101;   // R1
  Stacks[i][STACKSIZE-8] = 0x00000000;   // R0
  Stacks[i][STACKSIZE-9] = 0x11111111;   // R11
  Stacks[i][STACKSIZE-10] = 0x10101010;  // R10
  Stacks[i][STACKSIZE-11] = 0x09090909;  // R9
  Stacks[i][STACKSIZE-12] = 0x08080808;  // R8
  Stacks[i][STACKSIZE-13] = 0x07070707;  // R7
  Stacks[i][STACKSIZE-14] = 0x06060606;  // R6
  Stacks[i][STACKSIZE-15] = 0x05050505;  // R5
  Stacks[i][STACKSIZE-16] = 0x04040404;  // R4
}

//******** OS_RunPtrScheduler ***************
// Inputs: none
// Outputs: none
// move RunPtr to the next thread
void OS_RunPtrScheduler(void){
	RunPt = RunPt->next;
	while((RunPt->blockedState)){
	
	}
}

//******** OS_AddThread *************** 
// add a foregound thread to the scheduler
// Inputs: pointer to a void/void foreground task
//         number of bytes allocated for its stack
//         priority, 0 is highest, 5 is the lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// stack size must be divisable by 8 (aligned to double word boundary)
// In Lab 2, you can ignore both the stackSize and priority fields
// In Lab 3, you can ignore the stackSize fields
int OS_AddThread(void(*task)(void), uint32_t stackSize, 
					uint32_t priority){
	uint32_t status; 
  // put Lab 2 (and beyond) solution here
	status = StartCritical();
	if (TailPt==NULL) {												// initialization
		tcbs[THREAD_ID].next = &tcbs[THREAD_ID]; // set first node pointing to itself
		tcbs[THREAD_ID].prior = &tcbs[THREAD_ID];
		RunPt = &tcbs[THREAD_ID]; 							// initiate first task
		TailPt = &tcbs[THREAD_ID]; 							// initiate tail node
	} else {
		tcbType* orginNext = TailPt -> next;	// store the origin next node
		TailPt -> next = &tcbs[THREAD_ID]; 	// current node.next point to new node
		tcbs[THREAD_ID].next = orginNext; 	// new node.next points to origin next
		tcbs[THREAD_ID].prior = TailPt;
		orginNext->prior = &tcbs[THREAD_ID];
		orginNext = NULL; // clear tmp pointer
		TailPt = TailPt->next;
	}
	tcbs[THREAD_ID].id = THREAD_ID;			  // set the id field of the node
	tcbs[THREAD_ID].priority = priority;  // set the priority field of the node
	tcbs[THREAD_ID].blockedState = ACTIVE;
	OS_SetInitialStack(THREAD_ID); 
	Stacks[THREAD_ID][STACKSIZE-2] = (int32_t)(task); // PC
	THREAD_ID++;	// increment thread id for future new threads
	
  EndCritical(status);   
  return 1; // replace this line with solution
};

//******** OS_Id *************** 
// returns the thread ID for the currently running thread
// Inputs: none
// Outputs: Thread ID, number greater than zero 
uint32_t OS_Id(void){
  // put Lab 2 (and beyond) solution here
  
  return 0; // replace this line with solution
};


//******** OS_AddPeriodicThread *************** 
// add a background periodic task
// typically this function receives the highest priority
// Inputs: pointer to a void/void background function
//         period given in system time units (12.5ns)
//         priority 0 is the highest, 5 is the lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// You are free to select the time resolution for this function
// It is assumed that the user task will run to completion and return
// This task can not spin, block, loop, sleep, or kill
// This task can call OS_Signal  OS_bSignal  OS_AddThread
// This task does not have a Thread ID
// In lab 1, this command will be called 1 time
// In lab 2, this command will be called 0 or 1 times
// In lab 2, the priority field can be ignored
// In lab 3, this command will be called 0 1 or 2 times
// In lab 3, there will be up to four background threads, and this priority field 
//           determines the relative priority of these four threads
int OS_AddPeriodicThread(void(*task)(void), 
   uint32_t period, uint32_t priority){
  // put Lab 2 (and beyond) solution here
  Timer4A_Init(task, period * 80000, priority); // period per ms sampling
  return 0; // replace this line with solution
};





/*----------------------------------------------------------------------------
  PF1/4? Interrupt Handler
 *----------------------------------------------------------------------------*/
void GPIOPortF_Handler(void){
	GPIO_PORTF_ICR_R = 0x10;      // acknowledge flag4
	(*UserSW1Task)();               // execute user task
}

//******** OS_AddSW1Task *************** 
// add a background task to run whenever the SW1 (PF4) button is pushed
// Inputs: pointer to a void/void background function
//         priority 0 is the highest, 5 is the lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// It is assumed that the user task will run to completion and return
// This task can not spin, block, loop, sleep, or kill
// This task can call OS_Signal  OS_bSignal   OS_AddThread
// This task does not have a Thread ID
// In labs 2 and 3, this command will be called 0 or 1 times
// In lab 2, the priority field can be ignored
// In lab 3, there will be up to four background threads, and this priority field 
//           determines the relative priority of these four threads
int OS_AddSW1Task(void(*task)(void), uint32_t priority){
  // put Lab 2 (and beyond) solution here
	UserSW1Task = task; // user function
  SYSCTL_RCGCGPIO_R |= 0x00000020; // (a) activate clock for port F
//  FallingEdges = 0;             // (b) initialize counter
  GPIO_PORTF_DIR_R &= ~0x10;    // (c) make PF4 in (built-in button)
  GPIO_PORTF_AFSEL_R &= ~0x10;  //     disable alt funct on PF4
  GPIO_PORTF_DEN_R |= 0x10;     //     enable digital I/O on PF4   
  GPIO_PORTF_PCTL_R &= ~0x000F0000; // configure PF4 as GPIO
  GPIO_PORTF_AMSEL_R = 0;       //     disable analog functionality on PF
  GPIO_PORTF_PUR_R |= 0x10;     //     enable weak pull-up on PF4
  GPIO_PORTF_IS_R &= ~0x10;     // (d) PF4 is edge-sensitive
  GPIO_PORTF_IBE_R &= ~0x10;    //     PF4 is not both edges
  GPIO_PORTF_IEV_R &= ~0x10;    //     PF4 falling edge event
  GPIO_PORTF_ICR_R = 0x10;      // (e) clear flag4
  GPIO_PORTF_IM_R |= 0x10;      // (f) arm interrupt on PF4 *** No IME bit as mentioned in Book ***
  NVIC_PRI7_R = (NVIC_PRI7_R&0xFF00FFFF)|(priority<<21); // (g) user set priority
  NVIC_EN0_R = 0x40000000;      // (h) enable interrupt 30 in NVIC
  return 1; // replace this line with solution
};

//******** OS_AddSW2Task *************** 
// add a background task to run whenever the SW2 (PF0) button is pushed
// Inputs: pointer to a void/void background function
//         priority 0 is highest, 5 is lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// It is assumed user task will run to completion and return
// This task can not spin block loop sleep or kill
// This task can call issue OS_Signal, it can call OS_AddThread
// This task does not have a Thread ID
// In lab 2, this function can be ignored
// In lab 3, this command will be called will be called 0 or 1 times
// In lab 3, there will be up to four background threads, and this priority field 
//           determines the relative priority of these four threads
int OS_AddSW2Task(void(*task)(void), uint32_t priority){
  // put Lab 2 (and beyond) solution here
    
  return 0; // replace this line with solution
};

// ******** OS_Sleep ************
// place this thread into a dormant state
// input:  number of msec to sleep
// output: none
// You are free to select the time resolution for this function
// OS_Sleep(0) implements cooperative multitasking
void OS_Sleep(uint32_t sleepTime){
  // put Lab 2 (and beyond) solution here
	DisableInterrupts();
	RunPt->sleepCounter = (sleepTime+1)/2;
	RunPt->blockedState = SLEEP;
	
	tcbType* saveRunPt = RunPt;
	// remove from tcb cycles
	if(RunPt->next==RunPt){ // only one in tcb cycle
		RunPt = NULL;
		TailPt = NULL;
	}else{ 									// not empty tcb cycles
		if(TailPt==RunPt){
			TailPt = RunPt->next;
		}
		RunPt->next->prior = RunPt->prior;
		RunPt->prior->next = RunPt->next;
		RunPt = RunPt->prior;
	}
	// insert into queue
	if(headToSleepQueue==NULL){ // empty queue, the first pt
		headToSleepQueue = saveRunPt;
		headToSleepQueue->next = NULL;
		headToSleepQueue->prior = NULL;
	}else{											// not empty queue
		saveRunPt->next = headToSleepQueue->next;
		if(headToSleepQueue->next!=NULL){
			headToSleepQueue->next->prior = saveRunPt;
		}
		saveRunPt->prior = headToSleepQueue;
		headToSleepQueue->next = saveRunPt;
	}
	EnableInterrupts();
	ContextSwitch(); // questions here
};

// ******** OS_Kill ************
// kill the currently running thread, release its TCB and stack
// input:  none
// output: none
void OS_Kill(void){
  // put Lab 2 (and beyond) solution here
	DisableInterrupts();
	
  EnableInterrupts();   // end of atomic section 
  for(;;){};        // can not return
    
}; 

// ******** OS_Suspend ************
// suspend execution of currently running thread
// scheduler will choose another thread to execute
// Can be used to implement cooperative multitasking 
// Same function as OS_Sleep(0)
// input:  none
// output: none
void OS_Suspend(void){
  // put Lab 2 (and beyond) solution here
  ContextSwitch();
};
  
// ******** OS_Fifo_Init ************
// Initialize the Fifo to be empty
// Inputs: size
// Outputs: none 
// In Lab 2, you can ignore the size field
// In Lab 3, you should implement the user-defined fifo size
// In Lab 3, you can put whatever restrictions you want on size
//    e.g., 4 to 64 elements
//    e.g., must be a power of 2,4,8,16,32,64,128
void OS_Fifo_Init(uint32_t size){
  // put Lab 2 (and beyond) solution here
   
  
};

// ******** OS_Fifo_Put ************
// Enter one data sample into the Fifo
// Called from the background, so no waiting 
// Inputs:  data
// Outputs: true if data is properly saved,
//          false if data not saved, because it was full
// Since this is called by interrupt handlers 
//  this function can not disable or enable interrupts
int OS_Fifo_Put(uint32_t data){
  // put Lab 2 (and beyond) solution here

    return 0; // replace this line with solution
};  

// ******** OS_Fifo_Get ************
// Remove one data sample from the Fifo
// Called in foreground, will spin/block if empty
// Inputs:  none
// Outputs: data 
uint32_t OS_Fifo_Get(void){
  // put Lab 2 (and beyond) solution here
  
  return 0; // replace this line with solution
};

// ******** OS_Fifo_Size ************
// Check the status of the Fifo
// Inputs: none
// Outputs: returns the number of elements in the Fifo
//          greater than zero if a call to OS_Fifo_Get will return right away
//          zero or less than zero if the Fifo is empty 
//          zero or less than zero if a call to OS_Fifo_Get will spin or block
int32_t OS_Fifo_Size(void){
  // put Lab 2 (and beyond) solution here
   
  return 0; // replace this line with solution
};


// ******** OS_MailBox_Init ************
// Initialize communication channel
// Inputs:  none
// Outputs: none
void OS_MailBox_Init(void){
  // put Lab 2 (and beyond) solution here
  

  // put solution here
};

// ******** OS_MailBox_Send ************
// enter mail into the MailBox
// Inputs:  data to be sent
// Outputs: none
// This function will be called from a foreground thread
// It will spin/block if the MailBox contains data not yet received 
void OS_MailBox_Send(uint32_t data){
  // put Lab 2 (and beyond) solution here
  // put solution here
   

};

// ******** OS_MailBox_Recv ************
// remove mail from the MailBox
// Inputs:  none
// Outputs: data received
// This function will be called from a foreground thread
// It will spin/block if the MailBox is empty 
uint32_t OS_MailBox_Recv(void){
  // put Lab 2 (and beyond) solution here
 
  return 0; // replace this line with solution
};

// ******** OS_Time ************
// return the system time 
// Inputs:  none
// Outputs: time in 12.5ns units, 0 to 4294967295
// The time resolution should be less than or equal to 1us, and the precision 32 bits
// It is ok to change the resolution and precision of this function as long as 
//   this function and OS_TimeDifference have the same resolution and precision 
uint32_t OS_Time(void){
  // put Lab 2 (and beyond) solution here

  return 0; // replace this line with solution
};

// ******** OS_TimeDifference ************
// Calculates difference between two times
// Inputs:  two times measured with OS_Time
// Outputs: time difference in 12.5ns units 
// The time resolution should be less than or equal to 1us, and the precision at least 12 bits
// It is ok to change the resolution and precision of this function as long as 
//   this function and OS_Time have the same resolution and precision 
uint32_t OS_TimeDifference(uint32_t start, uint32_t stop){
  // put Lab 2 (and beyond) solution here

  return 0; // replace this line with solution
};


// ******** OS_CounterIncrement ************
// Increase the counter by 1 once
// Inputs:  none
// Outputs: none
// Self-created for lab1
void OS_CounterIncrement(void){
  ElapsedTimerCounter = ElapsedTimerCounter + 1;
}

// ******** OS_ClearMsTime ************
// sets the system time to zero (solve for Lab 1), and start a periodic interrupt
// Inputs:  none
// Outputs: none
// You are free to change how this works
void OS_ClearMsTime(void){
  // put Lab 1 solution here
	ElapsedTimerCounter = 0;
	uint32_t F1000HZ = (80000000/1000);
	Timer5A_Init(&OS_CounterIncrement, F1000HZ, 2);  // initialize timer5A (1000 Hz)
  //EnableInterrupts();
};

// ******** OS_MsTime ************
// reads the current time in msec (solve for Lab 1)
// Inputs:  none
// Outputs: time in ms units
// You are free to select the time resolution for this function
// For Labs 2 and beyond, it is ok to make the resolution to match the first call to OS_AddPeriodicThread
uint32_t OS_MsTime(void){
  // put Lab 1 solution here
  return ElapsedTimerCounter;
};


//******** OS_Launch *************** 
// start the scheduler, enable interrupts
// Inputs: number of 12.5ns clock cycles for each time slice
//         you may select the units of this parameter
// Outputs: none (does not return)
// In Lab 2, you can ignore the theTimeSlice field
// In Lab 3, you should implement the user-defined TimeSlice field
// It is ok to limit the range of theTimeSlice to match the 24-bit SysTick
void OS_Launch(uint32_t theTimeSlice){
  // put Lab 2 (and beyond) solution here
	NVIC_ST_RELOAD_R = theTimeSlice - 1;
	NVIC_ST_CTRL_R = 0X00000007;
	StartOS(); // start on the first task
};

//******** I/O Redirection *************** 
// redirect terminal I/O to UART

int fputc (int ch, FILE *f) { 
  UART_OutChar(ch);
  return ch; 
}

int fgetc (FILE *f){
  char ch = UART_InChar();  // receive from keyboard
  UART_OutChar(ch);         // echo
  return ch;
}

int OS_RedirectToFile(char *name){
  
  return 1;
}
int OS_RedirectToUART(void){
  
  return 1;
}

int OS_RedirectToST7735(void){
  
  return 1;
}

int OS_EndRedirectToFile(void){
  
  return 1;
}
