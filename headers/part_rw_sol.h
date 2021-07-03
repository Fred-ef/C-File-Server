/*  This implementation aims to solve the recurrent problem of partial reads/writes from varius types of streams,
    encountered when using the "write()" and "read()" system calls.
    What it does is, at the end of a write/read, check if there has been any error during the stream and if there
    is any byte left to write/read */

#ifndef part_rw_sol_h     // Multiple include protection
#define part_rw_sol_h

#include "err_cleanup.h"     // Including definitions of ERR and SUCCESS

int readn(int fd, void* ptr, int n);     // Wrapper function of the read SysCall addressing the partial read problem
int writen(int fd, void* ptr, int n);       // Wrapper function of the write SysCall drressing the partial write problem

#endif // part_rw_sol_h