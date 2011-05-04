#ifndef __ZT_FILESYS_LIB_H__
#define __ZT_FILESYS_LIB_H__

#include <stdio.h>
#include <stdlib.h>
#include "cfs/cfs.h"

#ifdef TRUE
#undef TRUE
#endif
#define TRUE 1

#ifdef FALSE
#undef FALSE
#endif
#define FALSE 0

#define MAX_SAMPLE_NUMBER 10
#define MAX_STORED_FILES 24
#define MAX_INT 2^16 -1
#define ERROR_INVALID_FD -2
#define ERROR_WRITE_FILE -3
#define ERROR_READ_FILE -4
#define ERROR_READ_SEEK -5
#define START_POSITION 0
#define DECIMAL_BASE 10
#define FILENAME_SIZE 16

typedef struct
{
  unsigned int readFile;
  int readFD;
  char[FILENAME_SIZE] readFileName;
 
  unsigned int writeFile;
  unsigned char writeSampleNumber;
  int writeFD;
  char[FILENAME_SIZE] writeFileName;
 
  int storedFiles;
} FileManager;

/* Functions */
char initFileManager();
int write(const void* data, int size);
int read(void* data, int size);
void updateReadFile();
char readSeek(int pos); 
void updateWriteFile();
char isValidFD(int fd);

#endif
