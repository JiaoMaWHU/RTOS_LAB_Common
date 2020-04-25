// *************Server.c**************
// Students implement this as part of EE445M/EE380L.12 Lab 1,2,3,4 
// High-level OS user interface
// 
// Runs on LM4F120/TM4C123
// Jonathan W. Valvano 1/18/20, valvano@mail.utexas.edu
#include <stdint.h>
#include <string.h> 
#include <stdbool.h> 
#include <stdlib.h>
#include <stdio.h>
#include "../RTOS_Labs_common/OS.h"
#include "../RTOS_Labs_common/ADC.h"
#include "../RTOS_Labs_common/ST7735.h"
#include "../RTOS_Labs_common/Server.h"
#include "../inc/ADCT0ATrigger.h"
#include "../inc/ADCSWTrigger.h"
#include "../RTOS_Labs_common/UART0int.h"
#include "../RTOS_Labs_common/eDisk.h"
#include "../RTOS_Labs_common/eFile.h"
#include "../RTOS_Labs_common/heap.h"
#include "../RTOS_Lab5_ProcessLoader/loader.h"
#include "../RTOS_Labs_common/esp8266.h"
#include "../RTOS_Labs_common/interpreter.h"

#define RSP_BUFFER_SIZE 64
#define PF1     (*((volatile uint32_t *)0x40025008))
#define PF2     (*((volatile uint32_t *)0x40025010))
#define PF3     (*((volatile uint32_t *)0x40025020))

extern uint32_t NumCreated;
extern char Response[RSP_BUFFER_SIZE];
extern char espBuffer[1024];
extern char cmdInput[CMD1SIZE];
extern char cmdInput2[CMD2SIZE];

static const ELFSymbol_t symtab[] = {
   { "ST7735_Message", ST7735_Message }
};

Sema4Type ServerSema;

char body[20];
char header[] = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n";

// rsp[0]: command for the instructions
// rsp[1]: first argument
// rsp[2]: second argument
// rsp[3]: third arguments
// rsp[3]: forth arguments

// decode rsp[0]:
// 0: ADC_In(void)
// 1: OS_Time(void)
// 2: ST7735_Message(uint32_t  d, uint32_t  l, char *pt, int32_t value)
// 3: LED_Toggle(uint32_t id, uint32_t value) 
// 4: eFile ..
// 5: exec_elf ..
char * (rsp[5]); // an array of 4 char pointers

char const string1[]="Filename = %s";
char const string2[]="File size = %lu bytes";
char const string3[]="Number of Files = %u";
char const string4[]="Number of Bytes = %lu";
void ESP_ReadAllFiles(void) {
	int length = 0;
	memset(espBuffer, 0, 1024*sizeof(char));
	DSTATUS status = eFile_Mount();
	if (status) {
		length += sprintf(espBuffer+length, "Failed to mount");
		OS_Kill();
	}
	
	status =  eFile_DOpen("");
	if (status) {
		
		length += sprintf(espBuffer+length, "Failed to open dir"); 
		OS_Kill();
	}
	char* name = ""; 
	unsigned long size;
	unsigned int num = 0;
  unsigned long total = 0;

	while(!eFile_DirNext(&name, &size)){
		length += sprintf(espBuffer+length, string1, name);
    length += sprintf(espBuffer+length, "  ");
    length += sprintf(espBuffer+length, string2, size);
    length += sprintf(espBuffer+length, "\n\r");
		total = total+size;
    num++;   
  }
  length += sprintf(espBuffer+length, string3, num);
  length += sprintf(espBuffer+length, "\n\r");
  length += sprintf(espBuffer+length, string4, total);
  length += sprintf(espBuffer+length,"\n\r");
	
	status =  eFile_DClose();
	if (status) {
		length += sprintf(espBuffer+length, "Failed to close dir"); 
		OS_Kill();
	}
	
  status = eFile_Close();
	if (status) {
		length += sprintf(espBuffer+length, "Failed to close"); 
		OS_Kill();
	}
	OS_Kill();
};

void ESP_ReadFileTask(void) {
	int length = 0;
	memset(espBuffer, 0, 1024*sizeof(char));
	// open file
	DSTATUS status =  eFile_Mount();
	if (status) {
		length += sprintf(espBuffer+length, "Failed to mount"); 
		OS_Kill();
	}
	
	status =  eFile_DOpen("");
	if (status) {
		length += sprintf(espBuffer+length,"Failed to open dir"); 
		OS_Kill();
	}
	
	status = eFile_ROpen(cmdInput);
	if (status) {
		length += sprintf(espBuffer+length, "Error! File not found \n\r");
		memset(cmdInput, 0, CMD1SIZE);
    OS_Kill();
	}
	
	char data;
	while (!eFile_ReadNext(&data)) {
     length += sprintf(espBuffer+length,"%c", data);
	}

	status =  eFile_DClose();
	if (status) {
		length += sprintf(espBuffer+length, "Failed to close dir"); 
		OS_Kill();
	}
	
	status = eFile_Close();
	if (status) {
		length += sprintf(espBuffer+length, "Failed to close \n\r");
		memset(cmdInput, 0, CMD1SIZE);
		OS_Kill();
	}
	memset(cmdInput, 0, CMD1SIZE);
	OS_Kill();
}

void ESP_WriteFileTask(void) {
	OS_RedirectToFile(cmdInput);
  printf("%s",cmdInput2);
  OS_EndRedirectToFile();
	memset(espBuffer, 0, 1024*sizeof(char));
	sprintf(espBuffer,"Write %s succeeded \n\r",cmdInput);
	memset(cmdInput, 0, CMD1SIZE);
	OS_Kill();
}

void ESP_RunProgramTask(void) {
	// mount fat and dir
	  ELFEnv_t env = { symtab, 1 };
		DSTATUS status = eFile_Mount();
		int length = 0;
	  if (status) {
		   length += sprintf(espBuffer+length, "Failed to mount"); 
	     OS_Kill();
	  }
	
    status =  eFile_DOpen("");
	  if (status) {
		   length += sprintf(espBuffer+length, "Failed to open dir"); 
		   OS_Kill();
	  }
		
		if (exec_elf(cmdInput, &env) == 1) {
			  length += sprintf(espBuffer+length, "exec elf succeeded"); OutCRLF();
		} else {
		    length += sprintf(espBuffer+length, "exec elf failed"); OutCRLF();
		}

		status = eFile_DClose();
	  if (status) {
		   length += sprintf(espBuffer+length, "Failed to close dir"); 
		   OS_Kill();
	  }
		status = eFile_Close();
		
		if (status) {
		   length += sprintf(espBuffer+length, "Failed to close file system"); 
		   OS_Kill();
	  }
		OS_Kill();
}

void Response_Parser(char* buffer) {
	uint16_t id = 0;
	for(uint16_t i = 0; i<5; i++){
		rsp[i] = NULL;
	}
	rsp[0] = strtok(buffer, ",");
	while(rsp[id]!=NULL && id<5){
		rsp[++id] = strtok(NULL, ",");
	}
}

void ServerRequest(void) {
  ST7735_DrawString(0,3,"Connected           ",ST7735_YELLOW); 	
	// Receive request
  if(!ESP8266_Receive(Response, 64)){
    ST7735_DrawString(0,3,"No request",ST7735_YELLOW); 
    ESP8266_CloseTCPConnection();
    OS_Signal(&ServerSema);
    OS_Kill();
  }
  Response_Parser(Response);
	
	// ADC_in() 
  if(strncmp(rsp[0], "0", 1) == 0) {
    
    sprintf(body, "ADC_value = %d\r\n", ADC_In());
			
		if(!ESP8266_SendBuffered(header)) printf("Failed/r/n");		
		if(!ESP8266_SendBuffered(body)) printf("Failed/r/n");
		if(!ESP8266_SendBufferedStatus()) printf("Failed/r/n");
	
	// OS_Time()
	} else if (strncmp(rsp[0], "1", 1) == 0) {
		
		sprintf(body, "OS_Time = %d\r\n", OS_MsTime());
		
		if(!ESP8266_SendBuffered(header)) printf("Failed/r/n");		
		if(!ESP8266_SendBuffered(body)) printf("Failed/r/n");
		if(!ESP8266_SendBufferedStatus()) printf("Failed/r/n"); 	
	
		// ST7735_Message()
  } else if (strncmp(rsp[0], "2", 1) == 0) {
		uint32_t d = atoi(rsp[1]);
		uint32_t l = atoi(rsp[2]); 
		char *pt = rsp[3];
		int32_t value = atoi(rsp[4]);
		
		ST7735_Message(d,l,pt,value);
		
		if(!ESP8266_SendBuffered(header)) printf("Failed/r/n");		
		if(!ESP8266_SendBufferedStatus()) printf("Failed/r/n"); 	
	
	// LED_toggle()
  } else if (strncmp(rsp[0], "3", 1) == 0) {
    // id: 1-3 represent PF1-PF3
		uint32_t id = atoi(rsp[1]);
		// action: 1 or 0, 1 is on, 0 is off
		uint32_t action = atoi(rsp[2]); 
		
    if (id == 1) {
		  if (action == 1) {
				PF1 |= 0x02;
			}	else if (action == 0) {
			  PF1 &= ~0x02;
			}				
		} else if (id == 2) {
		  if (action == 1) {
				PF2 |= 0x04;
			}	else if (action == 0) {
			  PF2 &= ~0x04;
			}	
		} else if (id == 3) {
		  if (action == 1) {
				PF3 |= 0x08;
			}	else if (action == 0) {
			  PF3 &= ~0x08;
			}	
		}
		
		if(!ESP8266_SendBuffered(header)) printf("Failed/r/n");		
		if(!ESP8266_SendBufferedStatus()) printf("Failed/r/n"); 	
		
    // eFile_XXX (show dir, read file, write file, create file, delete file)
		// rsp[1] determines the operation:
    // a - show dir
    // b - read file
    // c - write file

	} else if (strncmp(rsp[0], "4", 1) == 0) { 
		if ((strncmp(rsp[1], "a", 1) == 0)) {
			OS_AddThread(&ESP_ReadAllFiles, 128, 0);
			OS_Suspend();
			ESP8266_Send(espBuffer);		
		} else if ((strncmp(rsp[1], "b", 1) == 0)) {
			memcpy(cmdInput, rsp[2], strlen(rsp[2]));
			OS_AddThread(&ESP_ReadFileTask, 128, 0);
			OS_Suspend();
			ESP8266_Send(espBuffer);
		} else if ((strncmp(rsp[1], "c", 1) == 0)) {
			memcpy(cmdInput, rsp[2], strlen(rsp[2]));
		  memcpy(cmdInput2, rsp[3], strlen(rsp[3]));			
			OS_AddThread(&ESP_WriteFileTask, 128, 0);
			OS_Suspend();
			ESP8266_Send(espBuffer);
		} 
		// exec_elf()
	} else if (strncmp(rsp[0], "5", 1) == 0) { 
	  	memcpy(cmdInput, rsp[1], strlen(rsp[1]));
		  OS_AddThread(&ESP_RunProgramTask, 128, 0);
			OS_Suspend(); 
		  ESP8266_Send(espBuffer);
	} else {
    ST7735_DrawString(0,3,"Not a valid request",ST7735_YELLOW); 	
	}

	for (int i = 0; i < 5; i++) rsp[i] = NULL;
	ESP8266_Send("\n\r");
	ESP8266_CloseTCPConnection();
	OS_Signal(&ServerSema);
	OS_Kill();
}

void Server(void){ 
	OS_InitSemaphore(&ServerSema,0);
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
  if(!ESP8266_StartServer(80,600)) {  // port 80, 5min timeout
    ST7735_DrawString(0,2,"Server failure",ST7735_YELLOW); 
    OS_Kill();
  }  
  ST7735_DrawString(0,2,"Server started",ST7735_GREEN);	
	// starts the loop for 
  while(1){
    ESP8266_WaitForConnection();
    
    // Launch thread with higher priority to serve request
    if(OS_AddThread(&ServerRequest,128,1)) NumCreated++;
    
    // The ESP driver only supports one connection, wait for the thread to complete
    OS_Wait(&ServerSema);		
  }
}


