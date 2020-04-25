#include <stdint.h>
#include <string.h> 
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include "../RTOS_Labs_common/OS.h"
#include "../RTOS_Labs_common/Telnet.h"
#include "../RTOS_Labs_common/UART0int.h"
#include "../RTOS_Labs_common/esp8266.h"
#include "../RTOS_Labs_common/interpreter.h"
#include "../RTOS_Labs_common/ST7735.h"
#define BUFFER_SIZE 64
extern uint32_t NumCreated;
int TelnetServerID = -1;

Sema4Type TelnetServerSema;
char * (tel_rsp[5]); // an array of 4 char pointers


void TelnetServer(void){
	TelnetServerID = OS_Id();
	OS_InitSemaphore(&TelnetServerSema,0);
	if(!ESP8266_Init(true,false)) {  // verbose rx echo on UART for debugging
    ST7735_DrawString(0,1,"No Wifi adapter",ST7735_YELLOW); 
    OS_Kill();
  }
  // Get Wifi adapter version (for debugging)
  ESP8266_GetVersionNumber(); 
  // Connect to access point
  if(!ESP8266_Connect(true)) {  
    ST7735_DrawString(0,1,"No Wifi network",ST7735_YELLOW); 
    OS_Kill();
  }
  ST7735_DrawString(0,1,"Wifi connected",ST7735_GREEN);
  if(!ESP8266_StartServer(23,600)) {  // port 23, 5min timeout
    ST7735_DrawString(0,2,"Server failure",ST7735_YELLOW); 
    OS_Kill();
  }  
  ST7735_DrawString(0,2,"Server started",ST7735_GREEN);	
	// starts the loop for 
  while(1){
    ESP8266_WaitForConnection();
    
    // Launch thread with higher priority to serve request
		if(OS_AddThread(&TelnetServerRequest,128,1)) NumCreated++;
    
    // The ESP driver only supports one connection, wait for the thread to complete
    OS_Wait(&TelnetServerSema);		
  }
}

void TelnetServerRequest(void){
	ST7735_DrawString(0,3,"Connected           ",ST7735_YELLOW); 	
	// Receive request
	TelnetServerID = OS_Id(); 
	Interpreter(); 
	ESP8266_CloseTCPConnection(); 
	OS_Signal(&TelnetServerSema);
	OS_Kill();
}
