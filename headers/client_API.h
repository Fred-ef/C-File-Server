/* DESCRIZIONE API CLIENT */

#ifndef client_API.h
#define client_API.h

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "err_cleanup.h"
#include "definitions.h"
#include "util.h"

#define RECONNECT_TIMER 200     // time to wait between one connection attempt and another

extern short fd_sock;       // holds the file descriptor representing the connection to the server
extern char** conn_addr;        // holds the address to which the clients connect in the openConnection call


/* ########################## Main functions ################################## */

int openConnection(const char* sockname, int msec, const struct timespec abstime);

int closeConnection(const char* sockname);

int openFile(const char* pathname, int flags);

int readFile(const char* pathname, void** buf, size_t* size);

int readNFiles(int N, const char* dirname);

int writeFile(const char* pathname, const char* dirname);

int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname);

int lockFile(const char* pathname);

int unlockFile(const char* pathname);

int closeFile(const char* pathname);

int removeFile(const char* pathname);



/* ########################## Auxiliary functions ################################## */

static short sleep_ms(int);        // makes the caller sleep for 200ms

#endif // client_API.h