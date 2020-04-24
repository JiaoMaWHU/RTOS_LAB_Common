/**
 * @file      Server.h
 * @brief     Real Time Operating System for Labs 6
 * @details   EE445M/EE380L.6
 * @version   V1.0
 * @warning   AS-IS
 * @note      For more information see  http://users.ece.utexas.edu/~valvano/
 * @date      Jan 5, 2020

 ******************************************************************************/

#include <stdint.h> 

//---------------------ServerRequest---------------------
// parse the buffer into four section, same as CMD_Parser in the interpreter
// Input: buffer taken by the server
// Output: none
void ResponseParser(char* buffer);

//---------------------ServerRequest---------------------
// Output run sserver request
// Input: none
// Output: none
void ServerRequest(void);

/**
 * @details  Run the user interface.
 * @param  none
 * @return none
 * @brief  Server
 */
void Server(void);
