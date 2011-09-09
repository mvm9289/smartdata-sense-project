#include "zt-filesys-lib.h"
#include "zt-debug-lib.h"


/* Functions */
char initFileManager(FileManager *fman, char prefix, 
                     int maxSampleNumber, int maxStoredFiles)
{  
  char initOk = FALSE;
  
  fman->maxSampleNumber = (maxSampleNumber < 1)?MAX_SAMPLE_NUMBER:maxSampleNumber;
  fman->maxStoredFiles = (maxStoredFiles < 1)?MAX_STORED_FILES:maxStoredFiles;
  fman->prefix = prefix;
  
  fman->writeSampleNumber = 0;
  fman->writeFile = 0;
  fman->writeFileName[0] = fman->prefix;
  itoa(fman->writeFile, &fman->writeFileName[1], DECIMAL_BASE);
  fman->writeFD = cfs_open(fman->writeFileName, CFS_WRITE);

  fman->readFile = 0;
  fman->readFileName[0] = fman->prefix;
  itoa(fman->readFile, &fman->readFileName[1], DECIMAL_BASE); 
  fman->readFD = cfs_open(fman->readFileName, CFS_READ);
  
  fman->storedFiles = 0;

  initOk = isValidFD(fman, fman->readFD) && isValidFD(fman, fman->writeFD);
  #ifdef DEBUG_FILEMAN
	debug_filesys_init(fman);
  #endif
  return initOk;
}

int write(FileManager *fman, const void* data, int size)
{
  char fdOk;
  int writeBytes;
  
  fdOk = isValidFD(fman, fman->writeFD);
  
  if (fdOk == TRUE) {
    writeBytes = cfs_write(fman->writeFD, data, size);
    #ifdef DEBUG_FILEMAN
		debug_filesys_write(fman, writeBytes,fdOk);
    #endif
    if (writeBytes >= 0) {
      updateWriteFile(fman);
      return writeBytes;
    }
    else return ERROR_WRITE_FILE;
  }
  else {
    fman->writeFD = cfs_open(fman->writeFileName, CFS_WRITE | CFS_APPEND);
    #ifdef DEBUG_FILEMAN
		debug_filesys_write(fman, writeBytes,fdOk);
    #endif
    return ERROR_INVALID_FD;
  }
}

int read(FileManager *fman, void* data, int size)
{
  char fdOk;
  int readBytes;
  
  fdOk = isValidFD(fman, fman->readFD);
  
  if (fdOk == TRUE) {
    readBytes = cfs_read(fman->readFD, data, size);
    #ifdef DEBUG_FILEMAN
      debug_filesys_read(fman, readBytes,fdOk);
    #endif
    if (readBytes >= 0) return readBytes;
    else return ERROR_READ_FILE;
  }
  else {
    fman->readFD = cfs_open(fman->readFileName, CFS_READ);
    #ifdef DEBUG_FILEMAN
      debug_filesys_read(fman, readBytes,fdOk);
    #endif
    return ERROR_INVALID_FD;
  }
}

int updateReadFile(FileManager *fman)
{
  char fdOk;
  if (fman->storedFiles > 0) {
    cfs_close(fman->readFD);
    cfs_remove(fman->readFileName);
    fman->storedFiles--;
    fman->readFile++;
    fman->readFileName[0] = fman->prefix;
    itoa(fman->readFile, &fman->readFileName[1], DECIMAL_BASE);
    fman->readFD = cfs_open(fman->readFileName, CFS_READ);

    fdOk = isValidFD(fman, fman->readFD);
    #ifdef DEBUG_FILEMAN
    	debug_filesys_updateReadFile(fman,fdOk);
    #endif 
    if (fdOk == TRUE) return fman->readFD;
    else { 
      #ifdef DEBUG_FILEMAN
        debug_filesys_updateReadFile(fman,fdOk);
      #endif
      return ERROR_INVALID_FD;
    }
  }
  else {
    #ifdef DEBUG_FILEMAN
      debug_filesys_updateReadFile(fman,fdOk);
    #endif
    return ERROR_NO_FILES_STORED; 
  }
}

char readSeek(FileManager *fman, int pos) 
{
  char fdOk;
  int readFilePos;
  
  fdOk = isValidFD(fman, fman->readFD);
  
  if (fdOk == TRUE) {
    readFilePos = cfs_seek(fman->readFD, pos, CFS_SEEK_SET);
    #ifdef DEBUG_FILEMAN
      debug_filesys_readSeek(fman,readFilePos,fdOk);
    #endif
    if (readFilePos >= 0) return readFilePos; 
    else return ERROR_READ_SEEK;
  }
  else {
    fman->readFD = cfs_open(fman->readFileName, CFS_READ); 
    #ifdef DEBUG_FILEMAN
      debug_filesys_readSeek(fman,readFilePos,fdOk);
    #endif
    return ERROR_INVALID_FD;
  }  
} 

void updateWriteFile(FileManager* fman)
{
  fman->writeSampleNumber++;
  if (fman->writeSampleNumber >= fman->maxSampleNumber) {
    cfs_close(fman->writeFD);
    fman->writeFile++;
    fman->storedFiles++;
    fman->writeSampleNumber = 0;
    fman->writeFileName[0] = fman->prefix;
    itoa(fman->writeFile, &fman->writeFileName[1], DECIMAL_BASE);
    fman->writeFD = cfs_open(fman->writeFileName,  CFS_WRITE); 
  }
  
  if (fman->storedFiles >= fman->maxStoredFiles) {
    cfs_close(fman->readFD);
    cfs_remove(fman->readFileName);
    fman->storedFiles--; 
    fman->readFile++;
    fman->readFileName[0] = fman->prefix;
    itoa(fman->readFile, &fman->readFileName[1], DECIMAL_BASE);
    fman->readFD = cfs_open(fman->readFileName, CFS_READ);
  }
  #ifdef DEBUG_FILEMAN
	debug_filesys_updateWriteFile(fman);
  #endif
}

char isValidFD(FileManager *fman, int fd)
{
  if (fd >= 0) return TRUE;
  else return FALSE;  
}

int getStoredFiles(FileManager *fman)
{
  return fman->storedFiles;
}
