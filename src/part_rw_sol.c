#include "part_rw_sol.h"

// Wrapper function of the read SysCall handling the interrupted-reading problem
int readn(int source, char* buf, int toread) {
  int bleft;     // Bytes left to read
  int bread;     // Bytes read until now
  bleft=toread;     // Before the start of the stream, nothing has been read
  while(bleft>0) {
    if((bread=read(source, buf, toread))==ERR) {     // If an error verified
      if(bleft==toread) return ERR;     // If nothing has been read, return the error state
      else break;     // If the error happened during the stream of data, return the number of bytes read
    }
    else if(bread==0) break;
    bleft-=bread;
    buf+=bread;
  }
  return (toread-bleft);
}