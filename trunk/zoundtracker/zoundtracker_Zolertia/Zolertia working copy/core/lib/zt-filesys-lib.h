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
#define MAX_STORED_FILES 2


#define START_POSITION 0
#define DECIMAL_BASE 10
#define FILENAME_SIZE 6

#define ERROR_INVALID_FD -2
#define ERROR_WRITE_FILE -3
#define ERROR_READ_FILE -4
#define ERROR_READ_SEEK -5
#define ERROR_NO_FILES_STORED -6

typedef struct
{
  unsigned int readFile;
  int readFD;
  char readFileName[FILENAME_SIZE];
 
  unsigned int writeFile;
  unsigned char writeSampleNumber;
  int writeFD;
  char writeFileName[FILENAME_SIZE];
 
  int maxSampleNumber;
  int maxStoredFiles;
  int storedFiles;
  char prefix;
} FileManager;

/* Functions */

char initFileManager(FileManager *fman, char prefix, 
                     int maxSampleNumber, 
                     int maxStoredFiles);
/* [Functionality]
     This function initilizes the file manager.
     
   [Context]
     This function must be called in the initialization section of the
     process, beyond 'PROCESS_BEGIN()'.*/


int write(FileManager *fman, const void* data, int size);
/* [Functionality]
     This function stores into the current write file 'size' bytes 
     starting from the begining of 'data' buffer.
     
     Every call to 'write' function means one sample stored by
     the Mobile node. Every 'MAX_SAMPLE_NUMBER' the current write file
     is closed and a new write file is opened to write new data.
     
   [Context]
     This function is used to store a sample/data, measured from one 
     sensor, into the file system.*/


int read(FileManager *fman, void* data, int size);
/* [Functionality]
     This functions stores into 'data' buffer 'size' bytes starting 
     from the read offset of the current read file.
     
     Every call to 'read' updates the read offset of the file. 
     If you need to re-read data from the read file you must use the 
     'readSeek' function to move the read offset pointer of the file.
     
     If you want to delete the current read file and be able to read
     the next stored file of the Mobile node, you must use the 
     'updateReadFile' function. 
      
   [Context]
     This function is used to get a sample measure, previously stored 
     into the file system, to send it to the Basestation.*/
     

int updateReadFile(FileManager *fman);
/* [Functionality]
     This function deletes the current read file and prepares the next 
     stored file of the node to be readed.
     
     If there's no any stored file in the file system the current read
     file is not deleted.
        
   [Context]
     This function is used to delete from the file system a file sent 
     and acknowledged by the Basestation. The deleted file can not be
     recovered.*/


char readSeek(FileManager *fman, int pos); 
/* [Functionality]
     This function updates the read offset to the 'pos' position 
     starting from the begining (absolute position) of the current 
     read file.
   
   [Context]
     This function is used to re-read a sample/data already sent and 
     not acknowledged by the Basestation.*/
     

void updateWriteFile(FileManager *fman);
/* [Functionality]
     This function closes the current write file when the number of 
     writes reachs 'MAX_SAMPLE_NUMBER'. Then opens a new file to
     write the next samples.
     
     If the number of stored files reachs 'MAX_STORED_FILES' the oldest 
     writed file stored into the file system is deleted. This 
     functionality prevents the capacity's overflow of the file
     system and the bottleneck of packets sent through the network to 
     the Basestation.  
   
   [Context]
     This auxiliary function is used by 'write' function to manage 
     and update the internal state of the file system.*/


char isValidFD(FileManager *fman, int fd);
/* [Functionality]
     This function verifies that the current read/write file, pointed by 
     the 'fd' file descriptor, is correctly opened. 
     
   [Context]
     This auxiliary function is used on all read/write operations.*/


int getStoredFiles(FileManager *fman);
/* [Functionality]
     This function returns the value of 'storedFiles' attribute. 
     
   [Context]
     This function is used by applications to control how many files are
     ready to be readed (and possibly to be sent).*/
     
#endif
