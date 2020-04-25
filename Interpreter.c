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
#include "../RTOS_Labs_common/heap.h"
#include "../RTOS_Labs_common/Interpreter.h"
#include "../RTOS_Lab5_ProcessLoader/loader.h"
#include "../RTOS_Labs_common/esp8266.h"

#define CMD_BUFFER_SIZE 128
#define BUFFER_SIZE 64

char cmd_buffer[CMD_BUFFER_SIZE];  // global to assist in debugging

extern char cmdInput[CMD1SIZE];
extern char cmdInput2[CMD2SIZE];
extern int32_t MaxJitter;
extern uint32_t NumCreated;
//extern uint32_t DataLost;
//extern uint32_t PIDWork;
extern uint32_t MaxISRDisableTime;
extern uint32_t TotalISRDisableTime;
extern int TelnetServerID;
int getout = -1;

static const ELFSymbol_t symtab[] = {
   { "ST7735_Message", ST7735_Message }
};

void Interpreter_OutString(char *s){
	if(OS_Id() == TelnetServerID) {
		if(!ESP8266_Send(s)) { 
		// Error handling, close and kill 
			ST7735_DrawString(0,3,"Send error",ST7735_YELLOW); 
			getout = 1;
		}
	} else {
		UART_OutString(s); 
	}
}

void Interpreter_InString(char *s, uint16_t max){
	if(OS_Id() == TelnetServerID) {
		if(!ESP8266_Receive(cmd_buffer, CMD_BUFFER_SIZE)){
			ST7735_DrawString(0,3,"No request",ST7735_YELLOW); 
			getout = 1;
		}
	} else {
		UART_InString(cmd_buffer, CMD_BUFFER_SIZE-1); OutCRLF();
	}
}

void Interpreter_OutUDec(uint32_t n){
	if(OS_Id() == TelnetServerID) {
		char snum[5];
		sprintf(snum, "%d", n);
		if(!ESP8266_Send(snum)) { 
			// Error handling, close and kill 
			ST7735_DrawString(0,3,"Send error",ST7735_YELLOW); 
			getout = 1;
		}
	} else {
		UART_OutUDec(n); 
	}
}

//---------------------OutCRLF---------------------
// Output a CR,LF to UART to go to a new line
// Input: none
// Output: none
void OutCRLF(void){
	if(OS_Id() == TelnetServerID) {
		if(!ESP8266_Send("\n")) { 
		// Error handling, close and kill 
		}
	} else {
		UART_OutChar(CR);
		UART_OutChar(LF);
	}
}

// Print jitter histogram
void Jitter1(int32_t MaxJitter, uint32_t const JitterSize, uint32_t JitterHistogram[]){
	Interpreter_OutString("Jitter for A:"); OutCRLF();
	Interpreter_OutString("MaxJitter: "); Interpreter_OutUDec(MaxJitter); OutCRLF();
	Interpreter_OutString("JitterSize: "); Interpreter_OutUDec(JitterSize); OutCRLF();
	for(uint32_t i = 0; i<JitterSize; i++){
		Interpreter_OutUDec(i); UART_OutChar(' '); Interpreter_OutUDec(JitterHistogram[i]); OutCRLF();
	}
	OutCRLF();
}

void Jitter2(int32_t MaxJitter, uint32_t const JitterSize, uint32_t JitterHistogram[]){
  Interpreter_OutString("Jitter for B:"); OutCRLF();
	Interpreter_OutString("MaxJitter: "); Interpreter_OutUDec(MaxJitter); OutCRLF();
	Interpreter_OutString("JitterSize: "); Interpreter_OutUDec(JitterSize); OutCRLF();
	for(uint32_t i = 0; i<JitterSize; i++){
		Interpreter_OutUDec(i); UART_OutChar(' '); Interpreter_OutUDec(JitterHistogram[i]); OutCRLF();
	}
	OutCRLF();
}

void FormatTask(void) {
	DSTATUS status = eFile_Format();
	if (status) {
    Interpreter_OutString("File formatting failed"); 
	} else {
		Interpreter_OutString("File formatting succeeded"); 
	}
	OutCRLF();
	OS_Kill();
}

void CreateFileTask(void) {
	DSTATUS status = eFile_Create(cmdInput);
	if (status) Interpreter_OutString("Create failed");
	printf("Create %s succeeded \n\r",cmdInput); 
	memset(cmdInput, 0, CMD1SIZE);
	OS_Kill();
}

void ReadFileTask(void) {
	// open file
	DSTATUS status =  eFile_Mount();
	if (status) {
		Interpreter_OutString("Failed to mount"); 
		OS_Kill();
	}
	
	status =  eFile_DOpen("");
	if (status) {
		Interpreter_OutString("Failed to open dir"); 
		OS_Kill();
	}
	
	status = eFile_ROpen(cmdInput);
	if (status) {
		printf("Error! File not found \n\r");
		memset(cmdInput, 0, CMD1SIZE);
    OS_Kill();
	}
	
	char data;
	while (!eFile_ReadNext(&data)) {
		UART_OutChar(data);
	}

	status =  eFile_DClose();
	if (status) {
		Interpreter_OutString("Failed to close dir"); 
		OS_Kill();
	}
	
	status = eFile_Close();
	if (status) {
		printf("Failed to close \n\r");
		memset(cmdInput, 0, CMD1SIZE);
		OS_Kill();
	}
	memset(cmdInput, 0, CMD1SIZE);
	OS_Kill();
}

void WriteFileTask(void) {
	OS_RedirectToFile(cmdInput);
  printf("%s",cmdInput2);
  OS_EndRedirectToFile();
	printf("Write %s succeeded \n\r",cmdInput); 
	memset(cmdInput, 0, CMD1SIZE);
	OS_Kill();
}

void DeleteFileTask(void) {
	// mount fat and dir
	DSTATUS status = eFile_Mount();
	if (status) {
		Interpreter_OutString("Failed to mount"); 
		OS_Kill();
	}
	
	status = eFile_Delete(cmdInput);
	if (status == 1) {
	  Interpreter_OutString("Delete failed"); 
	} else if (status == 0) {
	  Interpreter_OutString("Delete succeeded"); 	
	} else if (status == 2) {
		Interpreter_OutString("No such file"); 
	}
	eFile_Close();
	memset(cmdInput, 0, CMD1SIZE);
	OutCRLF();
	OS_Kill();
}

void RunProgramTask(void) {
	// mount fat and dir
	  ELFEnv_t env = { symtab, 1 };
		DSTATUS status = eFile_Mount();
	  if (status) {
		   Interpreter_OutString("Failed to mount"); 
	     OS_Kill();
	  }
	
    status =  eFile_DOpen("");
	  if (status) {
		   Interpreter_OutString("Failed to open dir"); 
		   OS_Kill();
	  }
		
		if (exec_elf(cmdInput, &env) == 1) {
			  Interpreter_OutString("exec elf succeeded"); OutCRLF();
		} else {
		    Interpreter_OutString("exec elf failed"); OutCRLF();
		}

		status = eFile_DClose();
	  if (status) {
		   Interpreter_OutString("Failed to close dir"); 
		   OS_Kill();
	  }
		status = eFile_Close();
		
		if (status) {
		   Interpreter_OutString("Failed to close file system"); 
		   OS_Kill();
	  }
		OS_Kill();
}

char const stringA[]="Filename = %s";
char const stringB[]="File size = %lu bytes";
char const stringC[]="Number of Files = %u";
char const stringD[]="Number of Bytes = %lu";
void ReadAllFiles(void) {
	DSTATUS status = eFile_Mount();
	if (status) {
		Interpreter_OutString("Failed to mount"); 
		OS_Kill();
	}
	
	status =  eFile_DOpen("");
	if (status) {
		Interpreter_OutString("Failed to open dir"); 
		OS_Kill();
	}
	char* name = ""; 
	unsigned long size;
	unsigned int num = 0;
  unsigned long total = 0;

	while(!eFile_DirNext(&name, &size)){
    printf(stringA, name);
    printf("  ");
    printf(stringB, size);
    printf("\n\r");
    total = total+size;
    num++;    
  }
  printf(stringC, num);
  printf("\n\r");
  printf(stringD, total);
  printf("\n\r");
	
	status =  eFile_DClose();
	if (status) {
		Interpreter_OutString("Failed to close dir"); 
		OS_Kill();
	}
	
  status = eFile_Close();
	if (status) {
		Interpreter_OutString("Failed to close"); 
		OS_Kill();
	}
	OS_Kill();
};

//---------------------Output help instructions---------------------
// Output help instructions
// Input: none
// Output: none
void Output_Help(void){
	Interpreter_OutString("==== Use , to separate, don't enter extra spaces in cmd ===="); OutCRLF(); OutCRLF();
	Interpreter_OutString("lcd_t,[1],[2],[3]: output string [2] and value [3] to no.[1] line at the top side of the lcd"); OutCRLF(); OutCRLF();
	Interpreter_OutString("lcd_b,[1],[2],[3]: output string [2] and value [3] to no.[1] line at the bottom side of the lcd"); OutCRLF(); OutCRLF();
	Interpreter_OutString("lcd_clr: clear the LCD"); OutCRLF(); OutCRLF();
	Interpreter_OutString("adc_init,[1]: initilize adc with channel number [1]"); OutCRLF(); OutCRLF();
	Interpreter_OutString("adc_get: output the value of adc"); OutCRLF(); OutCRLF();
	Interpreter_OutString("clr_ms: clear the time counter and start counting"); OutCRLF(); OutCRLF();
	Interpreter_OutString("get_ms: output the time counter"); OutCRLF(); OutCRLF();
	Interpreter_OutString("get_metrics: output the performance metrics, use it only in Lab2"); OutCRLF(); OutCRLF();
	Interpreter_OutString("get_systime: output the total and max system runtime during ISR disabled"); OutCRLF(); OutCRLF();
	Interpreter_OutString("reset_systime: reset the total and max system runtime"); OutCRLF(); OutCRLF();	
	Interpreter_OutString("create_file, [1]: create a file with given name"); OutCRLF(); OutCRLF();
	Interpreter_OutString("read_file, [1]: read the content in an given file"); OutCRLF(); OutCRLF();
	Interpreter_OutString("write_file, [1], [2]: write to a file with name [1] with content [2]"); OutCRLF(); OutCRLF();
	Interpreter_OutString("delete_file, [1]: remove a given file"); OutCRLF(); OutCRLF();
	Interpreter_OutString("format_file : clear all files and data in the disk"); OutCRLF(); OutCRLF();
	Interpreter_OutString("show_files : print all file names in the directory"); OutCRLF(); OutCRLF();
	Interpreter_OutString("run_program, [1]: excute the input program name"); OutCRLF(); OutCRLF();
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
	Interpreter_OutString("Finished"); OutCRLF();
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
			Interpreter_OutString("ADC initilized successfully"); OutCRLF();
		}else{
			Interpreter_OutString("ADC initilized unsuccessfully"); OutCRLF();
		}
	}else if(!strcmp("lcd_clr", cmd[0])){
		ST7735_FillRect(0, 0, 21*6, 16*10, ST7735_BLACK);
	}else if(!strcmp("adc_get", cmd[0])){
		Interpreter_OutString("ADC value: "); 
		Interpreter_OutUDec(ADC_In()); OutCRLF();
	}else if(!strcmp("clr_ms", cmd[0])){
		OS_ClearMsTime();
		Interpreter_OutString("Cleared"); OutCRLF();
	}else if(!strcmp("get_ms", cmd[0])){
		Interpreter_OutString("Counter: "); 
		Interpreter_OutUDec(OS_MsTime()); OutCRLF();
	}else if(!strcmp("get_metrics", cmd[0])){
		ST7735_Message(0,4,"Timer-jitter= ",MaxJitter);
		ST7735_Message(0,5,"Number of Threads= ",NumCreated);
//		ST7735_Message(0,6,"DataPointLost= ",DataLost);
//		ST7735_Message(0,7,"PIDWork= ",PIDWork);
	}else if(!strcmp("get_systime", cmd[0])){
		Interpreter_OutString("Max System Time during ISR disabled is: "); 
		uint32_t DisplayISRDisable = TotalISRDisableTime / 80000;
		uint32_t ISRPercentage = (DisplayISRDisable * 10000) / OS_MsTime();
		Interpreter_OutUDec(MaxISRDisableTime);OutCRLF();
		Interpreter_OutString("Total System Time during ISR disabled is: "); 
		Interpreter_OutUDec(DisplayISRDisable);Interpreter_OutString(" ms");OutCRLF();        
		Interpreter_OutString("The percentage(1/10000) is: "); 
		Interpreter_OutUDec(ISRPercentage);OutCRLF();   	
	}else if(!strcmp("reset_systime", cmd[0])){
		MaxISRDisableTime = 0;
		TotalISRDisableTime = 0;
		Interpreter_OutString("Finished"); OutCRLF();	       
	}else if(!strcmp("create_file", cmd[0])) {
		memcpy(cmdInput, cmd[1], strlen(cmd[1]));
		OS_AddThread(&CreateFileTask, 128, 0);
		OS_Suspend();
	} else if(!strcmp("read_file", cmd[0])) {
		memcpy(cmdInput, cmd[1], strlen(cmd[1]));
		OS_AddThread(&ReadFileTask, 128, 0);
		OS_Suspend();
	} else if(!strcmp("write_file", cmd[0])) {
		memcpy(cmdInput, cmd[1], strlen(cmd[1]));
		memcpy(cmdInput2, cmd[2], strlen(cmd[2]));
		OS_AddThread(&WriteFileTask, 128, 0);
		OS_Suspend();
	}else if(!strcmp("delete_file", cmd[0])) {
		memcpy(cmdInput, cmd[1], strlen(cmd[1]));
		OS_AddThread(&DeleteFileTask, 128, 0);
		OS_Suspend();
	}else if (!strcmp("format_file", cmd[0])) {
		OS_AddThread(&FormatTask, 128, 0);
		OS_Suspend();
	} else if (!strcmp("run_program", cmd[0])) {
		memcpy(cmdInput, cmd[1], strlen(cmd[1]));
		OS_AddThread(&RunProgramTask, 128, 0);
		OS_Suspend();
	} else if(!strcmp("show_files", cmd[0])) {
		OS_AddThread(&ReadAllFiles,128,0);
		OS_Suspend();
	}else if(!strcmp("exit", cmd[0])) {
		if(OS_Id() == TelnetServerID) {
			getout = 1;
		}
	}
	else {
		Interpreter_OutString("Invalid command"); OutCRLF();
	}
}

// *********** Command line interpreter (shell) ************
void Interpreter(void){ 
	memset(cmd_buffer, 0, sizeof(cmd_buffer));
  //uint32_t n;
	Interpreter_OutString("Welcome to Interpreter, use 'help'."); OutCRLF();
	// starts the loop for 
  while(1){
    Interpreter_OutString("> ");
    Interpreter_InString(cmd_buffer, CMD_BUFFER_SIZE-1); 
		CMD_Parser(cmd_buffer, CMD_BUFFER_SIZE);
		memset(cmd_buffer, 0, sizeof(cmd_buffer));
		if(getout==1){
			getout = -1;
			break;
		}
  }
}


