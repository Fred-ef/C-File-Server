#include "parser.h"

// TODO provare ad aggiungere 1 alla lunghezza delle stringe generate dalle funzioni

// Takes as input the parameter to search, the file in which the parameter is to be searched and the variable in which to save its value
int parse(char* path, char* key, unsigned short* dest) {
  int tempErr;     // Variable used for temporary error codes
  int source;     // Declaration of File Descriptor
  if((source=open(path, RPERM, NULL))==ERR) {     // Assigning config file to the FD source + elementary error handling
    perror("Opening config file: ");
    return ERR;
  }

  sstat* fileStat=(sstat*)malloc(sizeof(sstat));     // Definition of stat struct to retrieve info from the config file
  if((tempErr=fstat(source, fileStat))==ERR) {     // Retrieving info from the config file into fileStat + elementary error handling
    perror("Retrieving config file's stats: ");
    cleanup_f(source, fileStat, NULL);
    return ERR;
  }
  int fileSize=(int)fileStat->st_size;     // Saving the config file's length, expressed in bytes, plus an overhead, in the fileSize var

  char* buffer=(char*)malloc((fileSize)*sizeof(char));
  if((tempErr=readn(source, buffer, fileSize))==ERR) {     // Uses the safe version of the read SC to read the config file and puts it in the buffer + elementary error handling
    perror("Reading from config file: ");
    cleanup_f(source, fileStat, buffer, NULL);
    return ERR;
  }

  char* saveptr1;     // Declaration of the saving pointer to use in the strtok_r call
  char* saveptr2;     // Declaration of the saving pointer to use in the strtok_r call
  char* token1;     // Declaration of the pointer used to memorize the current line in the config file in the strteok_r call
  char* token2;     // Declaration of the pointer used to memorize the current portion of the line in the config file in the strtok_r call
  char* isSubstring;     // String indicating TODO
  if((token1=strtok_r(buffer, "\n", &saveptr1))==NULL) {     // Getting the first line from the config file + elementary error handling
    // LOG THE ERROR TODO
    cleanup_f(source, fileStat, buffer, NULL);
    return ERR;
  }

  while(token1) {     // While token1!=NULL, untill the end of file
    if((token2=strtok_r(token1, "=", &saveptr2))==NULL) {     // Getting the first token from the config file + elementary error handling
      // LOG THE ERROR TODO
      cleanup_f(source, fileStat, buffer, NULL);
      return ERR;
    }

    while(token2) {     // While token2!=NULL, until the end of current line
      //                if((isSubstring=strstr(token2=strtok_r(NULL, "=", &saveptr1), key))==NULL) {     // If the desired parameter is not in the current token
      if((isSubstring=strstr(token2, key))==NULL) {
        token2=strtok_r(NULL, "\n", &saveptr2);     // Continue scanning
      }
      else {     // If the current token is the desired parameter
        token2=strtok_r(NULL, "\n", &saveptr2);     // Get the rvalue of the 'equals' operation
        *dest=atoi(token2);     // Convert the value of the parameter to the type of the provided container variable
        cleanup_f(source, fileStat, buffer, NULL);
        return SUCCESS;
      }
    }

    token1=strtok_r(NULL, "\n", &saveptr1);     // Getting next line of config file
  }
  cleanup_f(source, fileStat, buffer, NULL);
  return ERR;
}


// Functions in the same manner as parse(), but searches for a string value and saves it in the string passed as the third input parameter
int parse_s(char* path, char* key, char** dest) {
  int tempErr;     // Variable used for temporary error codes
  int source;     // Declaration of File Descriptor
  if((source=open(path, RPERM, NULL))==ERR) {     // Assigning config file to the FD source + elementary error handling
    perror("Opening config file: ");
    return ERR;
  }

  sstat* fileStat=(sstat*)malloc(sizeof(sstat));     // Definition of stat struct to retrieve info from the config file
  if((tempErr=fstat(source, fileStat))==ERR) {     // Retrieving info from the config file into fileStat + elementary error handling
    perror("Retrieving config file's stats: ");
    cleanup_f(source, fileStat, NULL);
    return ERR;
  }
  int fileSize=(int)fileStat->st_size;     // Saving the config file's length, expressed in bytes, plus an overhead, in the fileSize var

  char* buffer=(char*)malloc((fileSize)*sizeof(char));
  if((tempErr=readn(source, buffer, fileSize))==ERR) {     // Uses the safe version of the read SC to read the config file and puts it in the buffer + elementary error handling
    perror("Reading from config file: ");
    cleanup_f(source, fileStat, buffer, NULL);
    return ERR;
  }

  char* saveptr1;     // Declaration of the saving pointer to use in the strtok_r call
  char* saveptr2;     // Declaration of the saving pointer to use in the strtok_r call
  char* token1;     // Declaration of the pointer used to memorize the current line in the config file in the strteok_r call
  char* token2;     // Declaration of the pointer used to memorize the current portion of the line in the config file in the strtok_r call
  char* isSubstring;     // String indicating TODO
  if((token1=strtok_r(buffer, "\n", &saveptr1))==NULL) {     // Getting the first line from the config file + elementary error handling
    // LOG THE ERROR TODO
    cleanup_f(source, fileStat, buffer, NULL);
    return ERR;
  }

  while(token1) {     // While token1!=NULL, untill the end of file
    if((token2=strtok_r(token1, "=", &saveptr2))==NULL) {     // Getting the first token from the config file + elementary error handling
      // LOG THE ERROR TODO
      cleanup_f(source, fileStat, buffer, NULL);
      return ERR;
    }

    while(token2) {     // While token2!=NULL, until the end of current line
      //                if((isSubstring=strstr(token2=strtok_r(NULL, "=", &saveptr1), key))==NULL) {     // If the desired parameter is not in the current token
      if((isSubstring=strstr(token2, key))==NULL) {
        token2=strtok_r(NULL, "\n", &saveptr2);     // Continue scanning
      }
      else {     // If the current token is the desired parameter
        token2=strtok_r(NULL, "\n", &saveptr2);     // Get the rvalue of the 'equals' operation
        *dest=(char*)malloc((strlen(token2)+1)*sizeof(char));     // Allocating the input string to contain the parameter obtained
        strcpy(*dest, token2);     // Copying the newfound parameter into the destination string
        cleanup_f(source, fileStat, buffer, NULL);
        return SUCCESS;
      }
    }

    token1=strtok_r(NULL, "\n", &saveptr1);     // Getting next line of config file
  }
  cleanup_f(source, fileStat, buffer, NULL);
  return ERR;
}