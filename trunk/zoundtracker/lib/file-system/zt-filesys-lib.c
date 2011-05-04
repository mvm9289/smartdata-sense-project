#include "zt-filesys-lib.h"

static FileManager fman;

/* Functions */
char initFileManager() 
{  
    fman.readFile = 0;
    itoa(fman.readFile, fman.readFileName, DECIMAL_BASE); 
    fman->readFD = cfs_open(fman.readFileName, CFS_READ);
    
    fman.writeSampleNumber = 0;
    fman.writeFile = 0;
    itoa(fman.writeFile, fman.writeFileName, DECIMAL_BASE);
    fman.writeFD = cfs_open(fman.writeFileName, CFS_WRITE);
    
    fman.storedFiles = 0;

    return isValidFD(fman.readFD) && isValidFD(fman.writeFD);
}

int write(const void* data, int size)
{
  char fdOk;
  int writeBytes;
  
  fdOk = isValidFD(fman.writeFD);
  
  if (fdOk == TRUE) {
    writeBytes = cfs_write(fman.writeFD, data, size);
    if (writeBytes >= 0) {
      updateWriteFile();
      return writeBytes;
    }
    else return ERROR_WRITE_FILE;
  }
  else {
    fman.writeFD = cfs_open(fman.writeFileName, CFS_WRITE | CFS_APPEND);
    return ERROR_INVALID_FD;
  }
}

int read(void* data, int size)
{
  bool fdOk;
  int readBytes;
  
  fdOk = isValidFD(fman.readFD);
  
  if (fdOk == TRUE) {
    readBytes = cfs_read(fman.readFD, data, size);
    if (readBytes >= 0) return readBytes;
    else return ERROR_READ_FILE;
  }
  else {
    fman.readFD = cfs_open(fman.readFileName, CFS_READ);
    return ERROR_INVALID_FD;
  }
}

void updateReadFile()
{
  cfs_close(fman.readFD);
  cfs_remove(fman.readFileName);
  fman.storedFiles--;
  fman.readFile++;
  itoa(fman.readFile, fman.readFileName, DECIMAL_BASE);
  fman.readFD = cfs_open(fman.readFileName, CFS_READ);
}

char readSeek(int pos) 
{
  char fdOk;
  int readFilePos;
  
  fdOk = isValidFD(fman.readFD);
  
  if (fdOk == TRUE) {
    readFilePos = cfs_seek(fman.readFD, pos, CFS_SEEK_SET);
    if (readFilePos >= 0) return readFilePos; 
    else return ERROR_READ_SEEK;
  }
  else {
    fman.readFD = cfs_open(fman.readFileName, CFS_READ);
    return ERROR_INVALID_FD;
  }  
} 

void updateWriteFile(FileManager* fileman)
{
  if (fman.writeSampleNumber >= MAX_SAMPLE_NUMBER) {
    cfs_close(fman.writeFD);
    fman.writeFile++;
    fman.storedFiles++;
    fman.writeSampleNumber = 0;
  
    itoa(fman.writeFile, fman.writeFileName, DECIMAL_BASE);
    fman.writeFD = cfs_open(fman.writeFileName,  CFS_WRITE); 
  }
  else {
    fman.writeSampleNumber++;
  }
  
  if (fman.storedFiles >= MAX_STORED_FILES) {
    cfs_close(fman.readFD);
    cfs_remove(fman.readFileName);
    fman.storedFiles--; 
    fman.readFile++;
    itoa(fman.readFile, fman.readFileName, DECIMAL_BASE);
    fman.readFD = cfs_open(fman.readFileName, CFS_READ);
  }
}

char isValidFD(int fd)
{
  if (fd >= 0) return TRUE;
  else return FALSE;  
}
