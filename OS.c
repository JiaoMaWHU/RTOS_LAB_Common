// *************os.c**************
// EE445M/EE380L.6 Labs 1, 2, 3, and 4 
// High-level OS functions
// Students will implement these functions as part of Lab
// Runs on LM4F120/TM4C123
// Jonathan W. Valvano 
// Jan 12, 2020, valvano@mail.utexas.edu


#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "../inc/tm4c123gh6pm.h"
#include "../inc/CortexM.h"
#include "../inc/PLL.h"
#include "../inc/LaunchPad.h"
#include "../inc/Timer3A.h"
#include "../inc/Timer4A.h"
#include "../inc/Timer5A.h"
#include "../inc/WTimer0A.h"
#include "../inc/LaunchPad.h"
#include "../RTOS_Labs_common/OS.h"
#include "../RTOS_Labs_common/ST7735.h"
#include "../inc/ADCT0ATrigger.h"
#include "../RTOS_Labs_common/UART0int.h"
#include "../RTOS_Labs_common/eFile.h"
#include "../RTOS_Labs_common/heap.h"

// function definitions in osasm.s
void OS_DisableInterrupts(void); // Disable interrupts
void OS_EnableInterrupts(void);  // Enable interrupts
void StartOS(void);
void ContextSwitch(void);

// Performance Measurements 
int32_t MaxJitter = 0;             // largest time jitter between interrupts in usec
int32_t MaxJitter2 = 0;
char cmdInput[BYTE_PER_DIR_ENTRY_NAME];
char cmdInput2[128];
#define JITTERSIZE 64
#define NUMTHREAD 32
#define STACKSIZE 128
#define RUN 0
#define ACTIVE 1
#define SLEEP 2
#define BLOCK 3
#define FifoBufferSize 32
#define TASKSLOT 8

uint32_t OUTPUTMODE;

uint32_t const JitterSize=JITTERSIZE;
uint32_t JitterHistogram[JITTERSIZE]={0,};
uint32_t JitterHistogram2[JITTERSIZE]={0,};
uint32_t ElapsedTimerCounter = 0;
uint32_t OSTimeCounter = 0;

uint32_t Mail;
Sema4Type Send; // semaphore for mailbox
Sema4Type Ack; // semaphore for mailbox

Sema4Type CurrentFifoSize; // semaphore for mutex in OS_FIFO
uint32_t FifoBuffer[FifoBufferSize];
uint32_t inFifoId;
uint32_t outFifoId;

void (*fun_ptr_arr[TASKSLOT])(void);
uint32_t periodCounter = 0;
uint32_t periodicTasksPeriod[TASKSLOT];

tcbType tcbs[NUMTHREAD];
tcbType *RunPt = NULL;
tcbType *TailPt = NULL;
tcbType *NextPt = NULL;

int32_t Stacks[NUMTHREAD][STACKSIZE];
uint32_t threadIdMax = 0;

tcbType dummyHeadOfQueue;
tcbType* headToSleepQueue = &dummyHeadOfQueue; // store the pointer for sleep tcbType

int32_t restoredTcbStack[NUMTHREAD];
int32_t restoreTcbStackPt = -1;

tcbType *HeadPt = NULL;

uint32_t PreISRDisableTime;
uint32_t MaxISRDisableTime;
uint32_t TotalISRDisableTime;
uint32_t timeDiff;

pcbType* addThreadProcessPt = NULL;
uint32_t globalPid = 0;

void (*UserSW1Task)(void);   // user function
void (*UserSW2Task)(void);   // user function

// ******** OS_PreISRDisableTime ************
// reads the current time counter in Timer 5 GPTMTAR), register
// Inputs:  none
// Outputs: none
int Discount = 0;

#define PD0  (*((volatile uint32_t *)0x40007004))
	
// ******** OS_PreISRDisableTime ************
// reads the current time counter in Timer 5 GPTMTAR), register
// Inputs:  none
// Outputs: none
void OS_PreDisableISRTime(void){
	// Timer 5 has been enabled
	Discount = Discount +1;
	if (SYSCTL_RCGCTIMER_R >> 5 == 0x01 && Discount == 1) {
	  PreISRDisableTime = TIMER5_TAR_R;
		PD0 = 0x00;
	}
};

// ******** OS_PostISRDisableTime ************
// calculate the system time interval during the interrupt disable
// update max system time and total system time during intterupt disable
// Inputs:  none
// Outputs: none
void OS_PostDisableISRTime(void) {
	// each uint in 1ms/80000
	Discount = Discount-1;
	if (SYSCTL_RCGCTIMER_R >> 5 == 0x01 && Discount == 0) {
		uint32_t curTime = TIMER5_TAR_R;
		uint32_t readTair = TIMER5_TAILR_R;
		if (curTime <= PreISRDisableTime) {
		  timeDiff = PreISRDisableTime - curTime;
		} else {
			timeDiff = TIMER5_TAILR_R - curTime + PreISRDisableTime;
		}
		PD0 = 0x01;
		TotalISRDisableTime += timeDiff;
		PD0 = 0x00;		
		if (timeDiff > MaxISRDisableTime) {
			MaxISRDisableTime = timeDiff;
		}
		PD0 = 0x01;
	}
}

//void OS_InsertTcbRound(tcbType* taget_node){
//	// make sure interrupt has already been disabled
//	// insert after tailnode
//	if(TailPt==NULL){ // tailpt and runpt all null, means the only thread in OS
//		RunPt=taget_node;
//		TailPt=taget_node;
//		TailPt->next = TailPt;
//		TailPt->prior = TailPt;
//	}else{
//		taget_node->next = TailPt->next;
//		TailPt->next->prior = taget_node;
//		TailPt->next = taget_node;
//		taget_node->prior = TailPt;
//		TailPt = TailPt->next;
//	}
//	taget_node->state = ACTIVE;
//}

//void OS_removeTcbRound(void){
//	// make sure interrupt has already been disabled
//	if(TailPt==RunPt){
//		TailPt = RunPt->next;
//	}
//	RunPt->next->prior = RunPt->prior;
//	RunPt->prior->next = RunPt->next;
//}

void insertPriorityHelper(tcbType** head_node, tcbType* target_node){
	// make sure interrupt has already been disabled
	if(*head_node==NULL){
		*head_node=target_node;
		(*head_node)->next = NULL;
		(*head_node)->prior = NULL;
	}else{
		tcbType* next_node = (*head_node), *old_node = NULL;
		while(next_node!=NULL && next_node->priority <= target_node->priority){
			old_node = next_node;
			next_node = next_node->next;
		}
		if(next_node==(*head_node)){
			(*head_node) = target_node;
			target_node->next = next_node;
			target_node->prior = NULL;
			next_node->prior = target_node;
		}else if(next_node==NULL){
			// last node in the list
			old_node->next = target_node;
			target_node->next = NULL;
			target_node->prior = old_node;
		}else{
			target_node->next = old_node->next;
			target_node->prior = old_node;
			next_node->prior = target_node;
			old_node->next = target_node;
		}
		next_node = NULL;
		old_node = NULL;
	}
}

void removePriorityHelper(tcbType** head_node, tcbType* target_node){
	if((*head_node)==target_node){
		(*head_node) = (*head_node)->next;
		if((*head_node)!=NULL){
			(*head_node)->prior = NULL;
		}
	}else{
		target_node->prior->next = target_node->next;
		target_node->next->prior = target_node->prior;
		target_node->next = NULL;
		target_node->prior = NULL;
	}
}

/*------------------------
	traverse the queue of sleeping threads
	decrease the time count
	if meets zero, insert the thread in the cycle using tailnode 
------------------------*/
void traverseSleepQueue(void){
	tcbType* tmpQueueNode = headToSleepQueue->next;
	uint32_t status = StartCritical(); //disable interrupt
	while(tmpQueueNode!=NULL){
		tmpQueueNode->sleepCounter -= 1;
		if(tmpQueueNode->sleepCounter==0){
			tcbType* originNext = tmpQueueNode->next; // reserved for next loop
			// remove pt from queue
			// tail or head or center or only one
			tmpQueueNode->prior->next = tmpQueueNode->next;
			if(tmpQueueNode->next!=NULL){
				tmpQueueNode->next->prior = tmpQueueNode->prior;
			}
			// insert after tailnode
			insertPriorityHelper(&HeadPt, tmpQueueNode);
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
	NextPt = HeadPt;
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
	DisableInterrupts();
	UART_Init();
	ST7735_InitR(INITR_REDTAB); // LCD initialization
	PLL_Init(Bus80MHz);         // set processor clock to 80 MHz
	Heap_Init();
	for (int i = 0; i < TASKSLOT; i++) {
		fun_ptr_arr[i] = NULL;
		periodicTasksPeriod[i] = 0;
	}
	headToSleepQueue->next=NULL; // initialize the head pt to queue
	headToSleepQueue->prior=NULL; // initialize the head pt to queue
	HeadPt->next = NULL;
	HeadPt->prior = NULL;
	PreISRDisableTime = 0;
	MaxISRDisableTime = 0;
	TotalISRDisableTime = 0;
	OUTPUTMODE = OUTPUT_UART;
}; 



// ******** OS_InitSemaphore ************
// initialize semaphore 
// input:  pointer to a semaphore
// output: none
void OS_InitSemaphore(Sema4Type *semaPt, int32_t value){
  // put Lab 2 (and beyond) solution here
	uint32_t status = StartCritical(); //disable interrupt
	semaPt->Value = value;
	semaPt->tcbP = NULL;
	EndCritical(status);
}; 

// ******** OS_Wait ************
// decrement semaphore 
// Lab2 spinlock
// Lab3 block if less than zero
// input:  pointer to a counting semaphore
// output: none
void OS_Wait(Sema4Type *semaPt){
  // put Lab 2 (and beyond) solution here
	uint32_t status = StartCritical(); //disable interrupt
	semaPt->Value -= 1;
	if(semaPt->Value<0){
		RunPt->state = BLOCK;
		RunPt->blockSemaPt = semaPt;
		OS_Suspend();
	}
  EndCritical(status);
}; 

// ******** OS_Signal ************
// increment semaphore 
// Lab2 spinlock
// Lab3 wakeup blocked thread if appropriate 
// input:  pointer to a counting semaphore
// output: none
void OS_Signal(Sema4Type *semaPt){
  // put Lab 2 (and beyond) solution here
	long status = StartCritical(); 
	semaPt->Value += 1; 
	if(semaPt->Value<=0){
		tcbType* tmpPt = semaPt->tcbP;
		removePriorityHelper(&(semaPt->tcbP), tmpPt);
		insertPriorityHelper(&HeadPt, tmpPt);
		tmpPt = NULL;
	}
	EndCritical(status);
}; 

// ******** OS_bWait ************
// Lab2 spinlock, set to 0
// Lab3 block if less than zero
// input:  pointer to a binary semaphore
// output: none
void OS_bWait(Sema4Type *semaPt){
  // put Lab 2 (and beyond) solution here
	uint32_t status = StartCritical(); //disable interrupt
	semaPt->Value -= 1;
	if(semaPt->Value < 0){
		RunPt->state = BLOCK;
		RunPt->blockSemaPt = semaPt;
		OS_Suspend();
	}
  EndCritical(status);
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
	if(semaPt->Value <= 0){
		semaPt->Value += 1;
	}
	if(semaPt->Value<=0){
		tcbType* tmpPt = semaPt->tcbP;
		removePriorityHelper(&(semaPt->tcbP), tmpPt);
		insertPriorityHelper(&HeadPt, tmpPt);
		tmpPt = NULL;
	}
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


//******** OS_RunPtrScheduler ***************
// Inputs: none
// Outputs: none
// move RunPtr to the next thread
void OS_RunPtrScheduler(void){
	if(RunPt->state==BLOCK){
		removePriorityHelper(&HeadPt, RunPt);
		insertPriorityHelper(&(RunPt->blockSemaPt->tcbP), RunPt);
	}else if(RunPt->state==RUN){
		RunPt->state=ACTIVE;
	}
	// if sleep, just sleep state
	RunPt = NextPt;
	RunPt->state = RUN;
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
	uint32_t threadID;
	uint32_t status;
  // put Lab 2 (and beyond) solution here
	status = StartCritical();
	if(restoreTcbStackPt>=0){ 								// use restored tcb or not
		threadID = restoredTcbStack[restoreTcbStackPt];
		restoreTcbStackPt--;
	}else{
		threadID = threadIdMax;
		if(threadIdMax+1==NUMTHREAD){
			EndCritical(status);   
			return 0; // replace this line with solution
		}
		threadIdMax++;
	}
	tcbs[threadID].id = threadID;			  // set the id field of the node
	tcbs[threadID].priority = priority;  // set the priority field of the node
	tcbs[threadID].blockSemaPt = NULL;
	OS_SetInitialStack(threadID);
	Stacks[threadID][STACKSIZE-2] = (int32_t)(task); // PC
	if(addThreadProcessPt!=NULL){
		// add from AddProcess
		tcbs[threadID].processPt = addThreadProcessPt;
		addThreadProcessPt->threadSize++;
		addThreadProcessPt = NULL;
	}else{
		// add from OS or the thread of a process
		if(RunPt==NULL){ // add before OS_launch
			tcbs[threadID].processPt = NULL;
		}else{
			// add from a thread
			tcbs[threadID].processPt = RunPt->processPt;
		}
	}
	// set r9
	if(tcbs[threadID].processPt!=NULL){
		Stacks[threadID][STACKSIZE-11] = (int)tcbs[threadID].processPt->dataPt;
	}
	insertPriorityHelper(&HeadPt, &tcbs[threadID]);
	threadID++;	// increment thread id for future new threads
  EndCritical(status);   
  return 1; // replace this line with solution
};
			

int OS_AddParamThread(void(*task)(char *param), char *param, uint32_t stackSize, uint32_t priority){
	uint32_t threadID;
	uint32_t status;
  // put Lab 2 (and beyond) solution here
	status = StartCritical();
	if(restoreTcbStackPt>=0){ 								// use restored tcb or not
		threadID = restoredTcbStack[restoreTcbStackPt];
		restoreTcbStackPt--;
	}else{
		threadID = threadIdMax;
		if(threadIdMax+1==NUMTHREAD){
			EndCritical(status);   
			return 1; // replace this line with solution
		}
		threadIdMax++;
	}
	tcbs[threadID].id = threadID;			  // set the id field of the node
	tcbs[threadID].priority = priority;  // set the priority field of the node
	tcbs[threadID].blockSemaPt = NULL;
	OS_SetInitialStack(threadID);
	Stacks[threadID][STACKSIZE-8] = (int32_t) param;   // R0 
	Stacks[threadID][STACKSIZE-2] = (int32_t)(task); // PC
	insertPriorityHelper(&HeadPt, &tcbs[threadID]);
	threadID++;	// increment thread id for future new threads
  EndCritical(status);   
  return 1; // replace this line with solution
};
//******** OS_AddProcess *************** 
// add a process with foregound thread to the scheduler
// Inputs: pointer to a void/void entry point
//         pointer to process text (code) segment
//         pointer to process data segment
//         number of bytes allocated for its stack
//         priority (0 is highest)
// Outputs: 1 if successful, 0 if this process can not be added
// This function will be needed for Lab 5
// In Labs 2-4, this function can be ignored
int OS_AddProcess(void(*entry)(void), void *text, void *data, 
  unsigned long stackSize, unsigned long priority){
  // put Lab 5 solution here
	pcbType* newProcess = (pcbType*)Heap_Calloc(sizeof(pcbType));
	newProcess->dataPt = data;
	newProcess->textPt = text;
	newProcess->pid = globalPid++;
	newProcess->threadSize = 0;
	addThreadProcessPt = newProcess;
	// add initial thread
  return OS_AddThread(entry, stackSize, priority);
}

//******** OS_Id *************** 
// returns the thread ID for the currently running thread
// Inputs: none
// Outputs: Thread ID, number greater than zero 
uint32_t OS_Id(void){
  // put Lab 2 (and beyond) solution here
  return RunPt->id; // replace this line with solution
};

//******** AddPeriodicTaskHelper *************** 
// add a priority task to the function pointer array and initialize
// corresponding period in periodicTasksPeriod array.
// Inputs: task pointer, period, priority
// Outputs: None
void AddPeriodicTaskHelper(void(*task)(void), uint32_t period, uint32_t priority) {
	fun_ptr_arr[priority] = task;
	periodicTasksPeriod[priority] = (period / TIME_500US);
}

//******** periodicTask_Handler *************** 
// increment period counter and excutes a task when it is time
// Inputs: task pointer, period, priority
// Outputs: None
void PeriodicTask_Handler(void) {
	periodCounter++;
	for (int i = 0; i < TASKSLOT; i++) {
		if (fun_ptr_arr[i] != NULL && ((periodCounter % periodicTasksPeriod[i]) == 0)) {
			if(i==1){
				ComputeJitterA(periodicTasksPeriod[i] * TIME_500US);
			}else if(i==2){
				ComputeJitterB(periodicTasksPeriod[i] * TIME_500US);
			}
			(*(fun_ptr_arr[i]))();
		}
	}
}


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
	AddPeriodicTaskHelper(task, period, priority);
  return 1; // replace this line with solution
};

/*-----------------------------
Jitter for work A
--------------------------------*/
void ComputeJitterA(long PERIOD){
	unsigned static long LastTimeA;  // time at previous ADC sample
	uint32_t static TimeArunN = 0;  // time at previous ADC sample
  long thisTime = OS_Time();              // time at current ADC sample
	if(TimeArunN>0){
		long jitter;
		uint32_t diff = OS_TimeDifference(LastTimeA,thisTime);
		if(diff>PERIOD){
			jitter = (diff-PERIOD+4)/8;  // in 0.1 usec
		}else{
			jitter = (PERIOD-diff+4)/8;  // in 0.1 usec
		}
		if(jitter > MaxJitter){
			MaxJitter = jitter; // in usec
		}       // jitter should be 0
		if(jitter >= JitterSize){
			jitter = JitterSize-1;
		}
		JitterHistogram[jitter]++; 
	}
	TimeArunN++;
	LastTimeA = thisTime;
}

/*-----------------------------
Jitter for work B
--------------------------------*/
void ComputeJitterB(long PERIOD){
	unsigned static long LastTimeB;  // time at previous ADC sample
	uint32_t static TimeBrunN = 0;  // time at previous ADC sample
  long thisTime = OS_Time();              // time at current ADC sample
	if(TimeBrunN>0){
		long jitter;
		uint32_t diff = OS_TimeDifference(LastTimeB,thisTime);
		if(diff>PERIOD){
			jitter = (diff-PERIOD+4)/8;  // in 0.1 usec
		}else{
			jitter = (PERIOD-diff+4)/8;  // in 0.1 usec
		}
		if(jitter > MaxJitter2){
			MaxJitter2 = jitter; // in usec
		}       // jitter should be 0
		if(jitter >= JitterSize){
			jitter = JitterSize-1;
		}
		JitterHistogram2[jitter]++; 
	}
	TimeBrunN++;
	LastTimeB = thisTime;
}


/*----------------------------------------------------------------------------
  PF1/4? Interrupt Handler
 *----------------------------------------------------------------------------*/
void GPIOPortF_Handler(void){
	if(GPIO_PORTF_RIS_R&0x10){  // triggered by switch 1
		GPIO_PORTF_ICR_R = 0x10;  // acknowledge flag4
		(*UserSW1Task)(); 
  }
  if(GPIO_PORTF_RIS_R&0x01){  // triggered by switch 2
    GPIO_PORTF_ICR_R = 0x01;  // acknowledge flag0
		(*UserSW2Task)();               // execute user task
  }
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
	UserSW2Task = task; // user function
  SYSCTL_RCGCGPIO_R |= 0x00000020; // (a) activate clock for port F
//  FallingEdges = 0;             // (b) initialize counter
	GPIO_PORTF_LOCK_R = 0x4C4F434B; // unlock GPIO Port F
	GPIO_PORTF_CR_R |= 0x01;         // allow changes to PF4,0
  GPIO_PORTF_DIR_R &= ~0x01;    // (c) make PF0 in (built-in button)
  GPIO_PORTF_AFSEL_R &= ~0x01;  //     disable alt funct on PF0
  GPIO_PORTF_DEN_R |= 0x01;     //     enable digital I/O on PF0   
  GPIO_PORTF_PCTL_R &= ~0x0000000F; // configure PF0 as GPIO
  GPIO_PORTF_AMSEL_R &= ~0x01;       //     disable analog functionality on PF
  GPIO_PORTF_PUR_R |= 0x01;     //     enable weak pull-up on PF0
  GPIO_PORTF_IS_R &= ~0x01;     // (d) PF0 is edge-sensitive
  GPIO_PORTF_IBE_R &= ~0x01;    //     PF0 is not both edges
  GPIO_PORTF_IEV_R &= ~0x01;    //     PF0 falling edge event
  GPIO_PORTF_ICR_R = 0x01;      // (e) clear flag0
  GPIO_PORTF_IM_R |= 0x01;      // (f) arm interrupt on PF0 *** No IME bit as mentioned in Book ***
  NVIC_PRI7_R = (NVIC_PRI7_R&0xFF00FFFF)|(priority<<21); // (g) user set priority
  NVIC_EN0_R = 0x40000000;      // (h) enable interrupt 30 in NVIC
  return 1; // replace this line with solution
};

// ******** OS_Sleep ************
// place this thread into a dormant state
// input:  number of msec to sleep
// output: none
// You are free to select the time resolution for this function
// OS_Sleep(0) implements cooperative multitasking
void OS_Sleep(uint32_t sleepTime){
  // put Lab 2 (and beyond) solution here
	long status = StartCritical();
	RunPt->sleepCounter = (sleepTime+1)/2;
	RunPt->state = SLEEP;
	removePriorityHelper(&HeadPt, RunPt);
	NextPt = HeadPt;
	
	// insert into queue
	RunPt->next = headToSleepQueue->next;
	if(headToSleepQueue->next!=NULL){ // empty queue, the first pt
		headToSleepQueue->next->prior = RunPt;
	}
	headToSleepQueue->next = RunPt;
	RunPt->prior = headToSleepQueue;
	
	EndCritical(status);
	ContextSwitch(); // questions here
};

// ******** OS_Kill ************
// kill the currently running thread, release its TCB and stack
// input:  none
// output: none
void OS_Kill(void){
  // put Lab 2 (and beyond) solution here
	long status = StartCritical();
	removePriorityHelper(&HeadPt, RunPt);
	NextPt = HeadPt;
	restoreTcbStackPt++;
	restoredTcbStack[restoreTcbStackPt] = RunPt->id;
	if(RunPt->processPt!=NULL){
		// thread belongs to a process
		RunPt->processPt->threadSize--;
		if(RunPt->processPt->threadSize==0){
			// kill the process
			Heap_Free(RunPt->processPt->textPt);
			Heap_Free(RunPt->processPt->dataPt);
			Heap_Free(RunPt->processPt);
		}
	}
	EndCritical(status);
	ContextSwitch(); // questions here
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
	long status = StartCritical();
	NextPt = HeadPt;
	if(NextPt==RunPt){
		NextPt=NextPt->next;
	}
	EndCritical(status);
	
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
	OS_InitSemaphore(&CurrentFifoSize, 0);
	inFifoId = outFifoId = 0;
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
	if(CurrentFifoSize.Value == FifoBufferSize){
		return 0;
	}
	FifoBuffer[inFifoId] = data;
	inFifoId = (inFifoId+1)%FifoBufferSize;
	OS_Signal(&CurrentFifoSize);
  return 1; // replace this line with solution
};  

// ******** OS_Fifo_Get ************
// Remove one data sample from the Fifo
// Called in foreground, will spin/block if empty
// Inputs:  none
// Outputs: data 
uint32_t OS_Fifo_Get(void){
  // put Lab 2 (and beyond) solution here
  OS_Wait(&CurrentFifoSize);
	uint32_t data = FifoBuffer[outFifoId];
	outFifoId = (outFifoId+1)%FifoBufferSize;
  return data; // replace this line with solution
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
  return CurrentFifoSize.Value; // replace this line with solution
};


// ******** OS_MailBox_Init ************
// Initialize communication channel
// Inputs:  none
// Outputs: none
void OS_MailBox_Init(void){
  // put Lab 2 (and beyond) solution here
	OS_InitSemaphore(&Send, 0);
	OS_InitSemaphore(&Ack, 0);
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
	Mail = data;
	OS_Signal(&Send);
	OS_Wait(&Ack);
};

// ******** OS_MailBox_Recv ************
// remove mail from the MailBox
// Inputs:  none
// Outputs: data received
// This function will be called from a foreground thread
// It will spin/block if the MailBox is empty 
uint32_t OS_MailBox_Recv(void){
  // put Lab 2 (and beyond) solution here
	uint32_t theData;
	OS_Wait(&Send);
	theData = Mail;
	OS_Signal(&Ack);
  return theData; // replace this line with solution
};


// ******** OS_Time_Increament ************
// inreament the system time counter by 1
// triggered every 12.5ns
// Inputs:  none
// Outputs: time in 12.5ns units, 0 to 4294967295
void OS_Time_Increament(void) {
	OSTimeCounter = OSTimeCounter + 1;
}

// ******** OS_Time ************
// return the system time 
// Inputs:  none
// Outputs: time in 12.5ns units, 0 to 4294967295
// The time resolution should be less than or equal to 1us, and the precision 32 bits
// It is ok to change the resolution and precision of this function as long as 
//   this function and OS_TimeDifference have the same resolution and precision 
uint32_t OS_Time(void){
  // put Lab 2 (and beyond) solution here
  return OSTimeCounter*80; // replace this line with solution
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
  return stop - start; // replace this line with solution
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
	Timer5A_Init(&OS_CounterIncrement, TIME_1MS, 2);  // initialize timer5A (1000 Hz)
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
// It is ok to limit the range of theTimeSlice to match the 24-bit SysTik
void OS_Launch(uint32_t theTimeSlice){
  // put Lab 2 (and beyond) solution here
	NVIC_ST_RELOAD_R = theTimeSlice - 1;
	NVIC_ST_CTRL_R = 0X00000007;
	NVIC_SYS_PRI3_R = (NVIC_SYS_PRI3_R&0xFF00FFFF)|((uint32_t)(7)<<21);
	NVIC_SYS_PRI3_R = (NVIC_SYS_PRI3_R&0x00FFFFFF)|((uint32_t)(4)<<29);
	PreISRDisableTime = 0;
	MaxISRDisableTime = 0;
	TotalISRDisableTime = 0;
	Timer3A_Init(&OS_Time_Increament, TIME_1US, 0);
	Timer4A_Init(&PeriodicTask_Handler, TIME_500US, 1); // initialize periodic increament for background thread
	OS_ClearMsTime();
	RunPt = HeadPt;
	StartOS(); // start on the first task
};

//******** I/O Redirection *************** 
// redirect terminal I/O to UART

int fputc (int ch, FILE *f) { 
	if (OUTPUTMODE == OUTPUT_UART) {
	  UART_OutChar(ch);
	} else if (OUTPUTMODE == OUTPUT_FILE) {
		eFile_Write(ch);
	} else if (OUTPUTMODE == OUTPUT_LCD) {
		ST7735_OutChar(ch);
	}
  return ch; 
}

int fgetc (FILE *f){
  char ch = UART_InChar();  // receive from keyboard
  UART_OutChar(ch);         // echo
  return ch;
}

int OS_RedirectToFile(char *name){
  // mount dir and fat from disk
  if(eFile_Mount()) return 1;
	if(eFile_Create(name)) return 1;
	if(eFile_WOpen(name)) return 1;
	OUTPUTMODE = OUTPUT_FILE;
  return 0;
}
int OS_RedirectToUART(void){
	OUTPUTMODE = OUTPUT_UART;
  return 0;
}

int OS_RedirectToST7735(void){
  OUTPUTMODE = OUTPUT_LCD;
  return 0;
}

int OS_EndRedirectToFile(void){
  if(eFile_WClose()) return 1;
	OUTPUTMODE = OUTPUT_UART;
  return 0;
}
