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
#include "../RTOS_Labs_common/Client.h"

extern uint32_t NumCreated;
extern char cmd_buffer[CMD_BUFFER_SIZE];  
extern int getout;

char fetchBuffer[128];
char responseBuffer[512];

void Client_Help(void) {
	Interpreter_OutString("==== Use , to separate, don't enter extra spaces in cmd ===="); OutCRLF(); OutCRLF();
	Interpreter_OutString("adc_in: obtain adc sampling from the server launchpad"); OutCRLF(); OutCRLF();
	Interpreter_OutString("os_time: obtain os time of the server launchpad"); OutCRLF(); OutCRLF();
	Interpreter_OutString("st7735_message,[1],[2],[3],[4]: print on server lcd screen"); OutCRLF(); OutCRLF(); 
	Interpreter_OutString("led_on,[1]: trun on led specified by [1]. [1] ranges from 1-3"); OutCRLF(); OutCRLF(); 
	Interpreter_OutString("led_off,[1]: trun off led specified by [1]. [1] ranges from 1-3"); OutCRLF(); OutCRLF(); 	
	Interpreter_OutString("efile_all: read all availabe files on the server"); OutCRLF(); OutCRLF();
	Interpreter_OutString("efile_read,[1]: read a file. [1] represents file name"); OutCRLF(); OutCRLF(); 
	Interpreter_OutString("efile_write,[1],[2]: write a file. [1] represents the file name. [2] is the content"); OutCRLF(); OutCRLF();  	
	Interpreter_OutString("exec_elf, [1]: execute the file in the server. [1] is the axf file name"); OutCRLF(); OutCRLF(); 
	Interpreter_OutString("exit: exit the current client mode and return to normal interpreter"); OutCRLF(); OutCRLF(); 
}
 
void Client_CMD_Parser(char *cmd_buffer_, uint16_t length) {
	uint16_t id = 0;
	char * (cmd[5]); // an array of 4 char pointers
	for(uint16_t i = 0; i<5; i++){
		cmd[i] = NULL;
	}
	memset(fetchBuffer, 0, 128*sizeof(char));
	memset(responseBuffer, 0, 512*sizeof(char));
	cmd[0] = strtok(cmd_buffer_, ",");
	while(cmd[id]!=NULL && id<5){
		cmd[++id] = strtok(NULL, ",");
	}  
	
	if(!strcmp("help", cmd[0])){
		Client_Help();
	} else if(!strcmp("adc_in", cmd[0])){
		sprintf(fetchBuffer, "0,\r\n");
	} else if (!strcmp("os_time", cmd[0])) {
		sprintf(fetchBuffer, "1,\r\n");
	} else if (!strcmp("st7735_message", cmd[0])) {
		sprintf(fetchBuffer, "2, %d, %d, %s, %d,\r\n", 
		                     atoi(cmd[1]), atoi(cmd[2]), cmd[3], atoi(cmd[4]));		
	} else if (!strcmp("led_on", cmd[0])) {
		sprintf(fetchBuffer, "3, %d, 1\r\n", atoi(cmd[1]));
	} else if (!strcmp("led_off", cmd[0])) {
		sprintf(fetchBuffer, "3, %d, 0\r\n", atoi(cmd[1]));	
	} else if (!strcmp("efile_all", cmd[0])) {
		sprintf(fetchBuffer, "4,a\r\n");	
	} else if (!strcmp("efile_read", cmd[0])) {
		sprintf(fetchBuffer, "4,b,%s,\r\n", cmd[1]);	
	} else if (!strcmp("efile_write", cmd[0])) {
		sprintf(fetchBuffer, "4,c,%s,%s,\r\n", cmd[1], cmd[2]);	
	} else if (!strcmp("exec_elf", cmd[0])) {
		sprintf(fetchBuffer, "5,%s,\r\n", cmd[1]);	
	} else if (!strcmp("exit", cmd[0])) {
		Interpreter_OutString("exit client connection\r\n");
		getout = 1;
	} else {
		Interpreter_OutString("wrong cmd\r\n");
	}
	
	if (strlen(fetchBuffer) > 0) {
		// Send request to server
		if(!ESP8266_Send(fetchBuffer)){
			Interpreter_OutString("Send failed");
			ST7735_DrawString(0,2,"Send failed",ST7735_YELLOW); 
		}    
		
		// Receive response from server
		if(!ESP8266_Receive(responseBuffer, 512)) {
			Interpreter_OutString("No response from server");
			ST7735_DrawString(0,2,"No response",ST7735_YELLOW); 
		} else {
			// print response
			Interpreter_OutString(responseBuffer);
		}
	}
}

void ClientInterpreter(void) {
	memset(cmd_buffer, 0, sizeof(cmd_buffer));
  //uint32_t n;
  Interpreter_OutString("Welcome to Client Mode, use 'help'."); OutCRLF();
	// starts the loop for 
	getout = -1;
  while(1){
    Interpreter_OutString("> ");
    Interpreter_InString(cmd_buffer, CMD_BUFFER_SIZE-1); 
		Client_CMD_Parser(cmd_buffer, CMD_BUFFER_SIZE);
		memset(cmd_buffer, 0, sizeof(cmd_buffer));
		if(getout==1){
			getout = -1;
			return;
		}
  }
}

void ClientInterpreterWrapper(void) {
  ST7735_DrawString(0,2,"                 ",ST7735_YELLOW);
  ESP8266_GetStatus();  // debugging
  // Fetch weather from server
  if(!ESP8266_MakeTCPConnection("192.168.0.135", 2626, 0)){ // open socket to web server on port 80
    ST7735_DrawString(0,2,"Connection failed",ST7735_YELLOW); 
    OS_Kill();
  }
	ClientInterpreter();
	ESP8266_CloseTCPConnection();
	OS_AddThread(&Interpreter, 128, 1);
	OS_Kill();
}

void Client(void){
  // Initialize and bring up Wifi adapter  
  if(!ESP8266_Init(true,false)) {  // verbose rx echo on UART for debugging
    ST7735_DrawString(0,1,"No Wifi adapter",ST7735_YELLOW); 
    OS_Kill();
  }
  // Get Wifi adapter version (for debugging)
  ESP8266_GetVersionNumber(); 
  // Connect to access point
  if(!ESP8266_Connect(false)) {  
    ST7735_DrawString(0,1,"No Wifi network",ST7735_YELLOW); 
    OS_Kill();
  }
  ST7735_DrawString(0,1,"Wifi connected",ST7735_GREEN);
	
  if(OS_AddThread(&ClientInterpreterWrapper,128,1)) NumCreated++;
  // Kill thread (should really loop to check and reconnect if necessary
  OS_Kill(); 
}
