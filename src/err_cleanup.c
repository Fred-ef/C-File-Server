#include "err_cleanup.h"

void cleanup_f(int fd, ...) {
  va_list va;
  va_start(va, fd);
  void* temp;
  int errtemp;

  while((temp=va_arg(va, void*))!=NULL) free(temp);
  if((errtemp=close(fd))==ERR) {
    perror("Closing file descriptor: ");
    exit(EXIT_FAILURE);
  }
  va_end(va);
}

void cleanup_p(void* ptr, ...) {
  va_list va;
  va_start(va, ptr);
  void* temp;

  free(ptr);
  while((temp=va_arg(va, void*))!=NULL) free(temp);
  va_end(va);
}