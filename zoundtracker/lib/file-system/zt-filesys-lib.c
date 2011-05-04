/* Functions */
char initFileManager(FileManager* fileman) 
{
    char initOk;
    initOk = FALSE;
    
    fileman->readFile = 0;
    itoa(fileman->readFile, fileman->readFileName, DECIMAL_BASE); 
    fileman->readFD = cfs_open(fileman->readFileName, CFS_READ);
    
    fileman->writeFile = 0;
    itoa(fileman->writeFile, fileman->writeFileName, DECIMAL_BASE);
    fileman->writeSampleNumber = 0;
    fileman->writeFD = cfs_open(fileman->writeFileName, CFS_WRITE);
    
    fileman->storedFiles = 0;

    initOk = isValidFD(fileman->readFD) && isValidFD(fileman->writeFD);
    return initOk;
}

int write(FileManager* fileman, const void* data, int size)
{
  char fdOk;
  int writeBytes;
  
  fdOk = isValidFD(fileman->writeFD);
  
  if (fdOk == TRUE) {
    writeBytes = cfs_write(fileman->writeFD, data, size);
    if (writeBytes >= 0) {
      updateWriteFile(fileman);
      return writeBytes;
    }
    else return ERROR_WRITE_FILE;
  }
  else {
    fileman->writeFD = cfs_open(fileman->writeFileName, CFS_WRITE | CFS_APPEND);
    return ERROR_INVALID_FD;
  }
}

int read(FileManager* fileman, void* data, int size)
{
  bool fdOk;
  int readBytes;
  
  fdOk = isValidFD(fileman->readFD);
  
  if (fdOk == TRUE) {
    readBytes = cfs_read(fileman->readFD, data, size);
    if (readBytes >= 0) return readBytes;
    else return ERROR_READ_FILE;
  }
  else {
    fileman->readFD = cfs_open(fileman->readFileName, CFS_READ);
    return ERROR_INVALID_FD;
  }
}

void updateReadFile(FileManager* fileman)
{
  cfs_close(fileman->readFD);
  cfs_remove(fileman->readFileName);
  fileman->storedFiles--;
  fileman->readFile++;
  itoa(fileman->readFile, fileman->readFileName, DECIMAL_BASE);
  fileman->readFD = cfs_open(fileman->readFileName, CFS_READ);
}

char readSeek(FileManager* fileman, int pos) 
{
  char fdOk;
  int readFilePos;
  
  fdOk = isValidFD(fileman->readFD);
  
  if (fdOk == TRUE) {
    readFilePos = cfs_seek(fileman->readFD, pos, CFS_SEEK_SET);
    if (readFilePos >= 0) return readFilePos; 
    else return ERROR_READ_SEEK;
  }
  else {
    fileman->readFD = cfs_open(fileman->readFileName, CFS_READ);
    return ERROR_INVALID_FD;
  }  
} 

void updateWriteFile(FileManager* fileman)
{
  if (fileman->writeSampleNumber >= MAX_SAMPLE_NUMBER) {
    cfs_close(fileman->writeFD);
    fileman->writeFile++;
    fileman->storedFiles++;
    fileman->writeSampleNumber = 0;
  
    itoa(fileman->writeFile, fileman->writeFileName, DECIMAL_BASE);
    fileman->writeFD = cfs_open(fileman->writeFileName,  CFS_WRITE | CFS_APPEND); 
  }
  else {
    fileman->writeSampleNumber++;
  }
  
  if (fileman->storedFiles >= MAX_STORED_FILES) {
    cfs_close(fileman->readFD);
    cfs_remove(fileman->readFileName);
    fileman->storedFiles--; 
    fileman->readFile++;
    itoa(fileman->readFile, fileman->readFileName, DECIMAL_BASE);
    fileman->readFD = cfs_open(fileman->readFileName, CFS_READ);
  }
}

char isValidFD(int fd)
{
  if (fd >= 0) return TRUE;
  else return FALSE;  
}
