/**
 * @file      Interpreter.h
 * @brief     Real Time Operating System for Labs 2, 3 and 4
 * @details   EE445M/EE380L.6
 * @version   V1.0
 * @author    Valvano
 * @copyright Copyright 2020 by Jonathan W. Valvano, valvano@mail.utexas.edu,
 * @warning   AS-IS
 * @note      For more information see  http://users.ece.utexas.edu/~valvano/
 * @date      Jan 5, 2020

 ******************************************************************************/

#include <stdint.h> 

#define CMD_BUFFER_SIZE 128
#define BUFFER_SIZE 64

/**
 * @details  Run the user interface.
 * @param  none
 * @return none
 * @brief  Interpreter
 */
void Interpreter(void);

//---------------------OutCRLF---------------------
// Output a CR,LF to UART to go to a new line
// Input: none
// Output: none
void OutCRLF(void);

//---------------------CMD Parser---------------------
// Parse the string to specific command and execute it
// Input: string
// Output: none
void CMD_Parser(char *cmd_buffer_, uint16_t length);

//---------------------Output help instructions---------------------
// Output help instructions
// Input: none
// Output: none
void Output_Help(void);

//---------------------Call lcd function---------------------
// Output help instructions
// Input: cmd arrays
// Output: none
void Call_LCD(char *cmd[]);

void Interpreter_OutString(char *s);

void Interpreter_InString(char *s, uint16_t max);

//---------------------FileTaskSeries---------------------
// Output: none
void FormatTask(void);
void CreateFileTask(void);
void ReadFileTask(void);
void WriteFileTask(void);
void DeleteFileTask(void);
void RunProgramTask(void);
void ReadAllFiles(void);

