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

#define PD0  (*((volatile uint32_t *)0x40007004))
#define PD1  (*((volatile uint32_t *)0x40007008))
#define PD2  (*((volatile uint32_t *)0x40007010))
#define PD3  (*((volatile uint32_t *)0x40007020))

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
			ST7735_Message(1,1,"OS_MsTime: ", OS_MsTime());
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

void SW1Push1(void){
  if(OS_MsTime() > 20){ // debounce
    if(OS_AddThread(&ButtonWork1,128,1)){
      NumCreated++;
    }
    OS_ClearMsTime();  // at least 20ms between touches
  }
}

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
  unsigned long stackSize, unsigned long priority); 

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
  if(!OS_AddProcess(&TestUser,Heap_Calloc(128),Heap_Calloc(128),128,1)){
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
	return 0;
}

int Testmain2(void) {
	return 0;
}
// --------------------------------------------------

int main(void) { 			// main
  Testmain0();
}
