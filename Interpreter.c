// *************Interpreter.c**************
// Students implement this as part of EE445M/EE380L.12 Lab 1,2,3,4 
// High-level OS user interface
// 
// Runs on LM4F120/TM4C123
// Jonathan W. Valvano 1/18/20, valvano@mail.utexas.edu
#include <stdint.h>
#include <string.h> 
#include <stdlib.h>
#include <stdio.h>
#include "../RTOS_Labs_common/OS.h"
#include "../RTOS_Labs_common/ADC.h"
#include "../RTOS_Labs_common/ST7735.h"
#include "../inc/ADCT0ATrigger.h"
#include "../inc/ADCSWTrigger.h"
#include "../RTOS_Labs_common/UART0int.h"
#include "../RTOS_Labs_common/eDisk.h"
#include "../RTOS_Labs_common/eFile.h"

#define CMD_BUFFER_SIZE 128

// Print jitter histogram
void Jitter(int32_t MaxJitter, uint32_t const JitterSize, uint32_t JitterHistogram[]){
  // write this for Lab 3 (the latest)
	
}

//---------------------OutCRLF---------------------
// Output a CR,LF to UART to go to a new line
// Input: none
// Output: none
void OutCRLF(void){
  UART_OutChar(CR);
  UART_OutChar(LF);
}

<<<<<<< HEAD

void CMD_Identifier(char **cmd) {
	uint32_t line = 0;
	int32_t value = 0;
	if (strcmp(cmd[0], "lcd_top") == 0) {
			line = atoi(cmd[1]);
			value = atoi(cmd[3]); 
		  ST7735_Message(0, line, cmd[2], value);
	} else if (strcmp(cmd[0], "lcd_down") == 0) {
			line = atoi(cmd[1]);
			value = atoi(cmd[3]); 
		  ST7735_Message(1, line, cmd[2], value);
	}
	OutCRLF();
	UART_OutString("Done"); 
	OutCRLF();
}

void CMD_Parse(char* inputBuffer) {
	char *p = strtok(inputBuffer, ",");
	char *cmd[4];
	
	uint32_t i = 0;
	
	while (p != NULL && i < 4) {
		cmd[i++] = p;
		p = strtok(NULL, ",");
	}
  CMD_Identifier(cmd);
}

// *********** Command line interpreter (shell) ************
void Interpreter(void){
	uint32_t BUFFERSIZE = 128;
	char inputBuffer[BUFFERSIZE];
	memset(&inputBuffer[0],0, BUFFERSIZE);
	
	UART_OutString("Welcome!");  OutCRLF();
	
	while (1) {
		UART_OutChar('>');
		UART_InString(inputBuffer,19);
		CMD_Parse(inputBuffer);
		memset(&inputBuffer[0],0, BUFFERSIZE);
		OutCRLF();
	}
	
	
  // write this  
//	char i;
//  char string[20];  // global to assist in debugging
//  uint32_t n;
//	
//	
//  UART_OutChar('-');
//  UART_OutChar('-');

//  while(1){
//    UART_OutString("InString: ");
//    UART_InString(string,19);
//    UART_OutString(" OutString="); 
//		if (strcmp(string,"help") == 0) {
//			UART_OutString("Welcome to Help"); 
//			ST7735_OutString("Welcome to Help");
//		} else {
//			UART_OutString(string);
//		}

//		if (strcmp(string,"clear") == 0) {
//			ST7735_FillScreen(ST7735_BLACK);
//		}
//		
//		
//		OutCRLF();
		
//		uint32_t upDown;
//		uint32_t line;
//		int32_t value;

//    UART_OutString("Top or Down: ");  
//		upDown=UART_InUDec();
//		OutCRLF();

//		UART_OutString("line: ");  
//		line=UART_InUDec();
//		OutCRLF();
//		
//		UART_OutString("value: ");  
//		value=UART_InUDec();
//		OutCRLF();

//		ST7735_Message(upDown, line, "this is message: ", value);
//		
//			OutCRLF();
//  }
=======
//---------------------Output help instructions---------------------
// Output help instructions
// Input: none
// Output: none
void Output_Help(void){
	UART_OutString("==== Use , to separate, don't enter extra spaces in cmd ===="); OutCRLF(); OutCRLF();
	UART_OutString("lcd_t,[1],[2],[3]: output string [2] and value [3] to no.[1] line at the top side of the lcd"); OutCRLF(); OutCRLF();
	UART_OutString("lcd_b,[1],[2],[3]: output string [2] and value [3] to no.[1] line at the bottom side of the lcd"); OutCRLF(); OutCRLF();
	UART_OutString("lcd_clr: clear the LCD"); OutCRLF(); OutCRLF();
	UART_OutString("adc_init,[1]: initilize adc with channel number [1]"); OutCRLF(); OutCRLF();
	UART_OutString("adc_get: output the value of adc"); OutCRLF(); OutCRLF();
	UART_OutString("clr_ms: clear the time counter and start counting"); OutCRLF(); OutCRLF();
	UART_OutString("get_ms: output the time counter"); OutCRLF(); OutCRLF();
}

//---------------------Call lcd function---------------------
// Output help instructions
// Input: cmd arrays
// Output: none
void Call_LCD(char * (cmd[])){
	uint32_t side = (!strcmp("lcd_t", cmd[0]))? 0: 1;
	uint32_t line = atoi(cmd[1]);
	int32_t value = atoi(cmd[3]);
	ST7735_Message(side, line, cmd[2], value);
	UART_OutString("Finished"); OutCRLF();
}

//---------------------CMD Parser---------------------
// Parse the string to specific command and execute it
// Input: string
// Output: none
void CMD_Parser(char *cmd_buffer_, uint16_t length){
	uint16_t id = 0;
	char * (cmd[4]); // an array of 4 char pointers
	for(uint16_t i = 0; i<4; i++){
		cmd[i] = NULL;
	}
	cmd[0] = strtok(cmd_buffer_, ",");
	while(cmd[id]!=NULL && id<4){
		cmd[++id] = strtok(NULL, ",");
	}
	if(!strcmp("help", cmd[0])){
		Output_Help();
	}else if(!strcmp("lcd_t", cmd[0]) || !strcmp("lcd_b", cmd[0])){
		Call_LCD(cmd);
	}else if(!strcmp("adc_init", cmd[0])){
		int16_t res = -1;
		if(cmd[1]!=NULL){
			res = ADC_Init((uint32_t)atoi(cmd[1]));
		}
		if(res!=-1){
			UART_OutString("ADC initilized successfully"); OutCRLF();
		}else{
			UART_OutString("ADC initilized unsuccessfully"); OutCRLF();
		}
	}else if(!strcmp("lcd_clr", cmd[0])){
		ST7735_FillRect(0, 0, 21*6, 16*10, ST7735_BLACK);
	}else if(!strcmp("adc_get", cmd[0])){
		UART_OutString("ADC value: "); 
		UART_OutUDec(ADC_In()); OutCRLF();
	}else if(!strcmp("clr_ms", cmd[0])){
		OS_ClearMsTime();
		UART_OutString("Cleared"); OutCRLF();
	}else if(!strcmp("get_ms", cmd[0])){
		UART_OutString("Counter: "); 
		UART_OutUDec(OS_MsTime()); OutCRLF();
	}else{
		UART_OutString("Invalid command"); OutCRLF();
	}
}

// *********** Command line interpreter (shell) ************
void Interpreter(void){ 
  char cmd_buffer[CMD_BUFFER_SIZE];  // global to assist in debugging
	memset(cmd_buffer, 0, sizeof(cmd_buffer));
  //uint32_t n;
	
	UART_OutString("Welcome to RTOS, enter 'help' to list all the commands."); OutCRLF();
	// starts the loop for 
  while(1){
    UART_OutString("> ");
    UART_InString(cmd_buffer, CMD_BUFFER_SIZE-1); OutCRLF();
		CMD_Parser(cmd_buffer, CMD_BUFFER_SIZE);
		memset(cmd_buffer, 0, sizeof(cmd_buffer));
  }
>>>>>>> 0ea6f188f7707610d5b52bd78c25a93702dcea63
}


