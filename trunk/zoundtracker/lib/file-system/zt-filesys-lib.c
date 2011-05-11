#include "zt-filesys-lib.h"

static FileManager fman;

/* Functions */
char initFileManager() 
{  
  char initOk = FALSE;
  
  fman.writeSampleNumber = 0;
  fman.writeFile = 0;
  itoa(fman.writeFile, fman.writeFileName, DECIMAL_BASE);
  fman.writeFD = cfs_open(fman.writeFileName, CFS_WRITE);

  fman.readFile = 0;
  itoa(fman.readFile, fman.readFileName, DECIMAL_BASE); 
  fman.readFD = cfs_open(fman.readFileName, CFS_READ);
  
  fman.storedFiles = 0;

  initOk = isValidFD(fman.readFD) && isValidFD(fman.writeFD);
  #ifdef DEBUG_FILEMAN
    printf("[file-man] initFileManager:\n");
    printf("  readFile = %d\n  readFD = %d\n", fman.readFile, fman.readFD);
    printf("  writeFile = %d\n  writeFD = %d\n", fman.writeFile, fman.writeFD);
    printf("  writeSampleNumber = %d\n", fman.writeSampleNumber);
    printf("  storedFiles = %d\n\n", fman.storedFiles);
  #endif
  return initOk;
}

int write(const void* data, int size)
{
  char fdOk;
  int writeBytes;
  
  fdOk = isValidFD(fman.writeFD);
  
  if (fdOk == TRUE) {
    writeBytes = cfs_write(fman.writeFD, data, size);
    #ifdef DEBUG_FILEMAN
      printf("[file-man] write:\n");
      printf("  writeFile = %d\n  writeFD = %d\n", fman.writeFile, fman.writeFD);
      printf("  writeBytes = %d\n\n", writeBytes);
    #endif
    if (writeBytes >= 0) {
      updateWriteFile();
      return writeBytes;
    }
    else { 
      #ifdef DEBUG_FILEMAN
        printf("[file-man] write:\n");
        printf("  Error: File manager can't write into writeFile.\n\n");
      #endif
      return ERROR_WRITE_FILE;
    }
  }
  else {
    fman.writeFD = cfs_open(fman.writeFileName, CFS_WRITE | CFS_APPEND);
    #ifdef DEBUG_FILEMAN
      printf("[file-man] write:\n");
      printf("  Error: writeFD is not a valid file descriptor.\n\n");
    #endif
    return ERROR_INVALID_FD;
  }
}

int read(void* data, int size)
{
  char fdOk;
  int readBytes;
  
  fdOk = isValidFD(fman.readFD);
  
  if (fdOk == TRUE) {
    readBytes = cfs_read(fman.readFD, data, size);
    #ifdef DEBUG_FILEMAN
      printf("[file-man] read:\n");
      printf("  readFile = %d\n  readFD = %d\n", fman.readFile, fman.readFD);
      printf("  readBytes = %d\n\n", readBytes);
    #endif
    if (readBytes >= 0) return readBytes;
    else {
      #ifdef DEBUG_FILEMAN
        printf("[file-man] read:\n");
        printf("  Error: File manager can't read from readFile.\n\n");
      #endif
      return ERROR_READ_FILE;
    }
  }
  else {
    fman.readFD = cfs_open(fman.readFileName, CFS_READ);
    #ifdef DEBUG_FILEMAN
      printf("[file-man] read:\n");
      printf("  Error: readFD is not a valid file descriptor.\n\n");
    #endif
    return ERROR_INVALID_FD;
  }
}

int updateReadFile()
{
  if (fman.storedFiles > 0) {
    cfs_close(fman.readFD);
    cfs_remove(fman.readFileName);
    fman.storedFiles--;
    fman.readFile++;
    itoa(fman.readFile, fman.readFileName, DECIMAL_BASE);
    fman.readFD = cfs_open(fman.readFileName, CFS_READ);
    #ifdef DEBUG_FILEMAN
      printf("[file-man] updateReadFile:\n");
      printf("  readFile = %d\n  readFD = %d\n", fman.readFile, fman.readFD);
      printf("  storedFiles = %d\n\n", fman.storedFiles);
    #endif
    if (isValidFD(fman.readFD)) return fman.readFD;
    else { 
      #ifdef DEBUG_FILEMAN
        printf("[file-man] updateReadFile:\n");
        printf("  Error: readFD is not a valid file descriptor.\n\n");
      #endif
      return ERROR_INVALID_FD;
    }
  }
  else {
    #ifdef DEBUG_FILEMAN
      printf("[file-man] updateReadFile:\n");
      printf("  Error: There's no any file stored in the file system.\n\n");
    #endif
    return ERROR_NO_FILES_STORED; 
  }
}

char readSeek(int pos) 
{
  char fdOk;
  int readFilePos;
  
  fdOk = isValidFD(fman.readFD);
  
  if (fdOk == TRUE) {
    readFilePos = cfs_seek(fman.readFD, pos, CFS_SEEK_SET);
    #ifdef DEBUG_FILEMAN
      printf("[file-man] readSeek:\n");
      printf("  readFile = %d\n  readFD = %d\n", fman.readFile, fman.readFD);
      printf("  readFilePos = %d\n\n", readFilePos);
    #endif
    if (readFilePos >= 0) return readFilePos; 
    else {
      #ifdef DEBUG_FILEMAN
        printf("[file-man] readSeek:\n");
        printf("  Error: File manager can't update the read file offset pointer.\n\n");
      #endif
      return ERROR_READ_SEEK;
    }
  }
  else {
    fman.readFD = cfs_open(fman.readFileName, CFS_READ); 
    #ifdef DEBUG_FILEMAN
      printf("[file-man] readSeek:\n");
      printf("  Error: readFD is not a valid file descriptor.\n\n");
    #endif
    return ERROR_INVALID_FD;
  }  
} 

void updateWriteFile(FileManager* fileman)
{
  fman.writeSampleNumber++;
  if (fman.writeSampleNumber >= MAX_SAMPLE_NUMBER) {
    cfs_close(fman.writeFD);
    fman.writeFile++;
    fman.storedFiles++;
    fman.writeSampleNumber = 0;
    itoa(fman.writeFile, fman.writeFileName, DECIMAL_BASE);
    fman.writeFD = cfs_open(fman.writeFileName,  CFS_WRITE); 
  }
  
  if (fman.storedFiles >= MAX_STORED_FILES) {
    cfs_close(fman.readFD);
    cfs_remove(fman.readFileName);
    fman.storedFiles--; 
    fman.readFile++;
    itoa(fman.readFile, fman.readFileName, DECIMAL_BASE);
    fman.readFD = cfs_open(fman.readFileName, CFS_READ);
  }
  #ifdef DEBUG_FILEMAN
    printf("[file-man] updateWriteFile:\n");
    printf("  readFile = %d\n  readFD = %d\n", fman.readFile, fman.readFD);
    printf("  writeFile = %d\n  writeFD = %d\n", fman.writeFile, fman.writeFD);
    printf("  writeSampleNumber = %d\n", fman.writeSampleNumber);
    printf("  storedFiles = %d\n\n", fman.storedFiles);
  #endif
}

char isValidFD(int fd)
{
  if (fd >= 0) return TRUE;
  else return FALSE;  
}
