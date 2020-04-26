#ifndef CLIENT_H
#define CLIENT_H
#include <stdint.h>

void Client_Help(void);

void Client_CMD_Parser(char *cmd_buffer_, uint16_t length);

void ClientInterpreter(void);

void ClientInterpreterWrapper(void);

void Client(void);

#endif
