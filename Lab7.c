// Lab7.c

#include <stdint.h>
#include <stdbool.h> 
#include <stdio.h> 
#include <string.h>
#include <stdlib.h>
#include "../inc/tm4c123gh6pm.h"
#include "../inc/CortexM.h"
#include "../inc/LaunchPad.h"
#include "../inc/PLL.h"
#include "../inc/LPF.h"
#include "../RTOS_Labs_common/UART0int.h"
#include "../RTOS_Labs_common/ADC.h"
#include "../RTOS_Labs_common/OS.h"
#include "../RTOS_Labs_common/heap.h"
#include "../RTOS_Labs_common/Interpreter.h"
#include "../RTOS_Labs_common/ST7735.h"
#include "../RTOS_Labs_common/eDisk.h"
#include "../RTOS_Labs_common/can0.h"
#include "../RTOS_Labs_common/esp8266.h"
#include "../RTOS_Labs_common/Server.h"
#include "../RTOS_Labs_common/Telnet.h"

short IntTerm;     // accumulated error, RPM-sec
short PrevError;   // previous error, RPM

uint32_t NumCreated;   // number of foreground threads created
uint32_t IdleCount;    // CPU idle counter

char Response[64];
//---------------------User debugging-----------------------
extern int32_t MaxJitter;             // largest time jitter between interrupts in usec
extern uint16_t mpuEnable;

void SW1Push1(void);
void SW2Push2(void);

#define PD0  (*((volatile uint32_t *)0x40007004))
#define PD1  (*((volatile uint32_t *)0x40007008))
#define PD2  (*((volatile uint32_t *)0x40007010))
#define PD3  (*((volatile uint32_t *)0x40007020))
	
#define PF1  (*((volatile uint32_t *)0x40025008))
#define PF2  (*((volatile uint32_t *)0x40025010))
#define PF3  (*((volatile uint32_t *)0x40025020))

void PortD_Init(void){ 
  SYSCTL_RCGCGPIO_R |= 0x08;       // activate port D
  while((SYSCTL_RCGCGPIO_R&0x08)==0){};      
  GPIO_PORTD_DIR_R |= 0x0F;        // make PD3-0 output heartbeats
  GPIO_PORTD_AFSEL_R &= ~0x0F;     // disable alt funct on PD3-0
  GPIO_PORTD_DEN_R |= 0x0F;        // enable digital I/O on PD3-0
  GPIO_PORTD_PCTL_R = ~0x0000FFFF;
  GPIO_PORTD_AMSEL_R &= ~0x0F;;    // disable analog functionality on PD
}

void PortF_Init(void){ 
  SYSCTL_RCGCGPIO_R |= 0x20;       // activate port F
  while((SYSCTL_RCGCGPIO_R&0x20)==0){};      
  GPIO_PORTF_DIR_R |= 0x0F;        // make PD3-0 output heartbeats
  GPIO_PORTF_AFSEL_R &= ~0x0F;     // disable alt funct on PD3-0
  GPIO_PORTF_DEN_R |= 0x0F;        // enable digital I/O on PD3-0
  GPIO_PORTF_PCTL_R = ~0x0000FFFF;
  GPIO_PORTF_AMSEL_R &= ~0x0F;;    // disable analog functionality on PD
}

//------------------Idle Task--------------------------------
// foreground thread, runs when nothing else does
// never blocks, never sleeps, never dies
// inputs:  none
// outputs: none
void Idle(void){
  IdleCount = 0;          
  while(1) {
    IdleCount++;
    PD0 ^= 0x01;
		if (IdleCount == 5000000) {
//			ST7735_Message(1,1,"OS_MsTime: ", OS_MsTime());
			IdleCount = 0;
			OS_ClearMsTime();
		}
    WaitForInterrupt();
  }
}

//--------------end of Idle Task-----------------------------

//*******************final user main - bare bones OS, extend with your code**********
int realmain(void){ // realmain
  OS_Init();        // initialize, disable interrupts
  PortD_Init();     // debugging profile
  Heap_Init();      // initialize heap
  MaxJitter = 0;    // in 1us units
	
//  // hardware init
//  ADC_Init(0);  // sequencer 3, channel 0, PE3, sampling in Interpreter
//  CAN0_Open(RCV_ID,XMT_ID);    

//  // attach background tasks
//  OS_AddPeriodicThread(&disk_timerproc,TIME_1MS,0);   // time out routines for disk  

//  // create initial foreground threads
//  NumCreated = 0;
//  NumCreated += OS_AddThread(&Interpreter,128,2); 
//  NumCreated += OS_AddThread(&Idle,128,5);  // at lowest priority 
// 
//  OS_Launch(TIME_2MS); // doesn't return, interrupts enabled in here
  return 0;            // this never executes
}

void ButtonWork1(void){} 

// Process management test, add and reclaim dummy process
void TestUser(void){ uint32_t id; uint32_t time;
  id = OS_Id();
  PD2 ^= 0x04;
  ST7735_Message(0,1, "Hello world: ", id);
  time = OS_Time();
  OS_Sleep(1000);
  time = (((OS_TimeDifference(time, OS_Time()))/1000ul)*125ul)/10000ul;
  ST7735_Message(0,2, "Sleep time: ", time);
  PD2 ^= 0x04;
  OS_Kill();
}

//  OS-internal OS_AddProcess function
extern int OS_AddProcess(void(*entry)(void), void *text, void *data, 
  unsigned long stackSize, unsigned long priority, uint16_t groupId);

void TestProcess(void){ heap_stats_t heap1, heap2;
  // simple process management test, add process with dummy code and data segments
  ST7735_DrawString(0, 0, "Process test         ", ST7735_WHITE);
  printf("\n\rEE445M/EE380L, Lab 5 Process Test\n\r");
  PD1 ^= 0x02;
  if(Heap_Stats(&heap1)) OS_Kill();
  PD1 ^= 0x02;
  ST7735_Message(1,0,"Heap size  =",heap1.size); 
  ST7735_Message(1,1,"Heap used  =",heap1.used);  
  ST7735_Message(1,2,"Heap free  =",heap1.free);
  ST7735_Message(1,3,"Heap waste =",heap1.size - heap1.used - heap1.free);
  PD1 ^= 0x02;
  if(!OS_AddProcess(&TestUser,Heap_Calloc(128),Heap_Calloc(128),128,1, 1)){
    printf("OS_AddProcess error");
    OS_Kill();
  }
  PD1 ^= 0x02;
  OS_Sleep(2000); // wait long enough for user thread and hence process to exit/die
  PD1 ^= 0x02;
  if(Heap_Stats(&heap2)) OS_Kill();
  PD1 ^= 0x02;
  ST7735_Message(1,0,"Heap size  =",heap2.size); 
  ST7735_Message(1,1,"Heap used  =",heap2.used);  
  ST7735_Message(1,2,"Heap free  =",heap2.free);
  ST7735_Message(1,3,"Heap waste =",heap2.size - heap2.used - heap2.free);
  PD1 ^= 0x02;
  if((heap1.free != heap2.free)||(heap1.used != heap2.used)){
    printf("Process management heap error");
    OS_Kill();
  }
  printf("Successful process test\n\r");
  ST7735_DrawString(0, 0, "Process test successful", ST7735_YELLOW);
  OS_Kill();  
}

extern int32_t* heapP;
void TestGroupProcess(void){ heap_stats_t heap1, heap2;
  // simple process management test, add process with dummy code and data segments
  ST7735_DrawString(0, 0, "Process test         ", ST7735_WHITE);
  printf("\n\rEE445M/EE380L, Lab 5 Process Test\n\r");
  PD1 ^= 0x02;
  if(Heap_Stats(&heap1)) OS_Kill();
  PD1 ^= 0x02;
  ST7735_Message(1,0,"Heap size  =",heap1.size); 
  ST7735_Message(1,1,"Heap used  =",heap1.used);  
  ST7735_Message(1,2,"Heap free  =",heap1.free);
  ST7735_Message(1,3,"Heap waste =",heap1.size - heap1.used - heap1.free);
  PD1 ^= 0x02;
	if(!OS_AddProcess(&TestUser,Heap_Group_Calloc(128,2),Heap_Group_Calloc(128,2),128,1, 2)){
    printf("OS_AddProcess error");
    OS_Kill();
  }  
	if(!OS_AddProcess(&TestUser,Heap_Group_Calloc(128,1),Heap_Group_Calloc(128,1),128,1, 1)){
    printf("OS_AddProcess error");
    OS_Kill();
  }
//	if(!OS_AddProcess(&TestUser,Heap_Group_Calloc(128,1),Heap_Group_Calloc(128,1),128,1, 1)){
//    printf("OS_AddProcess error");
//    OS_Kill();
//  }
//	if(!OS_AddProcess(&TestUser,Heap_Group_Calloc(128,2),Heap_Group_Calloc(128,2),128,1, 2)){
//    printf("OS_AddProcess error");
//    OS_Kill();
//  }

  PD1 ^= 0x02;
  OS_Sleep(2000); // wait long enough for user thread and hence process to exit/die
  PD1 ^= 0x02;
  if(Heap_Stats(&heap2)) OS_Kill();
  PD1 ^= 0x02;
  ST7735_Message(1,0,"Heap size  =",heap2.size); 
  ST7735_Message(1,1,"Heap used  =",heap2.used);  
  ST7735_Message(1,2,"Heap free  =",heap2.free);
  ST7735_Message(1,3,"Heap waste =",heap2.size - heap2.used - heap2.free);
  PD1 ^= 0x02;
  if((heap1.free != heap2.free)||(heap1.used != heap2.used)){
    printf("Process management heap error");
    OS_Kill();
  }
  printf("Successful process test\n\r");
  ST7735_DrawString(0, 0, "Process test successful", ST7735_YELLOW);
  OS_Kill();  
}

void SW1Push1(void){
  if(OS_MsTime() > 20){ // debounce
    if(OS_AddThread(&TestProcess,128,1)){
      NumCreated++;
    }
    OS_ClearMsTime();  // at least 20ms between touches
  }
}

void SW2Push2(void){
  if(OS_MsTime() > 20){ // debounce
    if(OS_AddThread(&TestProcess,128,1)){
      NumCreated++;
    }
    OS_ClearMsTime();  // at least 20ms between touches
  }
}

int Testmain0(void){   // Testmain0
  OS_Init();           // initialize, disable interrupts
  PortD_Init();

  // attach background tasks
  OS_AddSW1Task(&SW1Push1,2);  // PF4, SW1
  OS_AddSW2Task(&SW2Push2,2);  // PF0, SW2
  
  // create initial foreground threads
  NumCreated = 0;
  NumCreated += OS_AddThread(&TestProcess,128,1);  
  NumCreated += OS_AddThread(&Idle,128,3); 
 
  OS_Launch(10*TIME_1MS); // doesn't return, interrupts enabled in here
  return 0;               // this never executes
}


int Testmain1(void) {
	OS_Init();           // initialize, disable interrupts
  PortD_Init();

	mpuEnable = 1;
  // attach background tasks
  OS_AddSW1Task(&SW1Push1,2);  // PF4, SW1
  OS_AddSW2Task(&SW2Push2,2);  // PF0, SW2
  
  // create initial foreground threads
  NumCreated = 0;
  NumCreated += OS_AddThread(&TestGroupProcess,128,1); 
  NumCreated += OS_AddThread(&Idle,128,3); 
 
  OS_Launch(10*TIME_1MS); // doesn't return, interrupts enabled in here
  return 0;               // this never executes
}
 
void dummy1(void) {
	//for (int i = 0; i < 100000; i++) {
    printf("Hello from dummy1 \r\n");
	//}
	OS_Kill();
}

void dummy2(void) {
	//for (int i = 0; i < 100000; i++) {
    printf("Hello from dummy2 \r\n");	
	//}
	OS_Kill();
}

int Testmain2(void) {
	OS_Init();           // initialize, disable interrupts
  PortD_Init();

  // attach background tasks
  OS_AddSW1Task(&SW1Push1,2);  // PF4, SW1
  OS_AddSW2Task(&SW2Push2,2);  // PF0, SW2
  
  // create initial foreground threads
  NumCreated = 0;
 
	OS_AddProcess(&dummy1,Heap_Group_Calloc(128,1),Heap_Group_Calloc(128,1),128,1, 1);
	OS_AddProcess(&dummy2,Heap_Group_Calloc(128,2),Heap_Group_Calloc(128,2),128,1, 2);	
	NumCreated += OS_AddThread(&Interpreter,128,3); 
  NumCreated += OS_AddThread(&Idle,128,4); 
  OS_Launch(10*TIME_1MS); // doesn't return, interrupts enabled in here
  return 0;               // this never executes	
}
// --------------------------------------------------
extern group groupArray[];
extern tcbType *RunPt;

void PortF_trigger(void){
	PF2 ^= 0x04;
	OS_Sleep(10);
	PF2 ^= 0x04;
	OS_Kill();
}

void victimLed(void){
	while(true){
		OS_AddGroupThread(&PortF_trigger, 128, 2, OS_GetGroupId());
		OS_Sleep(100);
		if(RunPt->processPt->textPt==NULL){
			OS_Kill();
		}
	}
}

void malware(void){
	int group_id = OS_GetGroupId();
	if(group_id==0){
		ST7735_Message(0,0,"No group, exit.",-1);
	}else{
		int victim_id = group_id==1?2:1;
		ST7735_Message(group_id-1,0,"Malware on group",group_id);
		ST7735_Message(group_id-1,1,"Detecting group",victim_id);
		int32_t* start = groupArray[victim_id].heapAddress;
		if(start!=NULL){
			int32_t size = sizeof(pcbType)/sizeof(int32_t);
			int32_t* end = start + HEAP_SIZE/2-1;
			int16_t flag = 0;
			while(start<end){
				if(*start==size){
					flag = 1;
					int threas = *(start+2);
					*(start+2) = threas - 1;
					break;
				}
				start = start + abs(*start) + 2;
			}
			if(flag){
				ST7735_Message(group_id-1,2,"PCB found:",(int)start);
				ST7735_Message(group_id-1,3,"Modified!",victim_id);
			}else{
				ST7735_Message(group_id-1,2,"PCB not found on",victim_id);
			}
		}else{
			ST7735_Message(group_id-1,2,"No victim groups",-1);
		}
		ST7735_Message(group_id-1,4,"Exit",-1);
	}
	OS_Kill();
}

void SW1PushDemo1(void){
  if(OS_MsTime() > 20){ // debounce
    if(OS_AddGroupThread(&malware,128,1,1)){
      NumCreated++;
    }
    OS_ClearMsTime();  // at least 20ms between touches
  }
}

void SW2PushDemo1(void){
  if(OS_MsTime() > 20){ // debounce
    if(OS_AddGroupThread(&malware,128,1,2)){
      NumCreated++;
    }
    OS_ClearMsTime();  // at least 20ms between touches
  }
}

void demo1(void){
	OS_Init();           // initialize, disable interrupts
  PortD_Init();
	PortF_Init();

  // attach background tasks
  OS_AddSW1Task(&SW1PushDemo1,2);
	OS_AddSW2Task(&SW2PushDemo1,2);
  
  // create initial foreground threads
  NumCreated = 0;
	
	NumCreated += OS_AddThread(&Interpreter,128,3); 
  NumCreated += OS_AddThread(&Idle,128,4); 
  OS_Launch(10*TIME_1MS); // doesn't return, interrupts enabled in here
}

extern tcbType* HeadPt;

void printProcesses(void){
	ST7735_FillRect(0, 0, 21*6, 16*10, ST7735_BLACK);
	tcbType* tmp = HeadPt;
	int ri = 0;
	char str[24];
	while(tmp!=NULL){
		if(tmp->processPt==NULL){
			ST7735_Message(0, ri++, "OS thread, Group:", 0);
		}else{
			sprintf(str, "Pid: %d, Group:", tmp->processPt->pid);
			ST7735_Message(0, ri++, str, tmp->groupPt->id);
		}
		tmp = tmp->next;
	}
	ST7735_Message(0,ri++,"End",-1);
	OS_Kill();
}

void idleProcess(void){
	while(true){
		for (int i = 0; i<10000; i++){}
		OS_Sleep(100);
	}
}

void SW1PushDemo2(void){
	if(OS_MsTime() > 200){ // debounce
    if(OS_AddThread(&printProcesses,128,1)){
      NumCreated++;
    }
    OS_ClearMsTime();  // at least 20ms between touches
  }
}


int32_t attack_pid = -1;
void malware2(void){
	int32_t* start = groupArray[1].heapAddress;
	int32_t size = sizeof(pcbType)/sizeof(int32_t);
	int32_t* end = start + HEAP_SIZE/2-1;
	int16_t flag = 0;
	while(start<end){
		if(*start==size){
			flag = 1;
			*(start+1) = attack_pid;
		}
		start = start + abs(*start) + 2;
	}
	if(flag){
		printf("Attack Done!\r\n");
	}else{
		printf("No process to attack.\r\n");
	}
	OS_Kill();
}

void demo2(void){
	OS_Init();           // initialize, disable interrupts
  PortD_Init();
	PortF_Init();

  // attach background tasks
  OS_AddSW1Task(&SW1PushDemo2,2);
  
  // create initial foreground threads
  NumCreated = 0;
	
	NumCreated += OS_AddThread(&Interpreter,128,3); 
  NumCreated += OS_AddThread(&Idle,128,4); 
  OS_Launch(10*TIME_1MS); // doesn't return, interrupts enabled in here
}


extern char cmdInput[CMD1SIZE];
extern char cmdInput2[CMD2SIZE];
void calculator(void) {
  int value = atoi(cmdInput) + atoi(cmdInput2);
	memset(cmdInput, 0, CMD1SIZE);
	memset(cmdInput2, 0, CMD2SIZE);	
	int group_id = OS_GetGroupId();
  OS_SharedMem_Put(value);	
	OS_Kill();
}

void printer(void) {
  int value = OS_SharedMem_Get();    
  printf("value: %d \r\n", value);
//	ST7735_Message(0,4,"result is ",value);		
	OS_Kill();
}

int main(void) { 			// main
  demo2();
  return 0;   
}
