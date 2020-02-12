// *************Interpreter.c**************
// Students implement this as part of EE445M/EE380L.12 Lab 1,2,3,4 
// High-level OS user interface
// 
// Runs on LM4F120/TM4C123
// Jonathan W. Valvano 1/18/20, valvano@mail.utexas.edu
#include <stdint.h>
#include <string.h> 
#include <stdio.h>
#include "../RTOS_Labs_common/OS.h"
#include "../RTOS_Labs_common/ST7735.h"
#include "../inc/ADCT0ATrigger.h"
#include "../inc/ADCSWTrigger.h"
#include "../RTOS_Labs_common/UART0int.h"
#include "../RTOS_Labs_common/eDisk.h"
#include "../RTOS_Labs_common/eFile.h"


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
}


