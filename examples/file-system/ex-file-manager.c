#include "contiki.h"
#include "cfs/cfs.h"
#include <stdio.h>
#include <string.h>
#include "lib/zt-filesys-lib.h"

#define READ_BUFFER_SIZE 10
#define WRITE_BUFFER_SIZE 1

static char read_buffer[READ_BUFFER_SIZE];
static char write_buffer;
static int i, write_bytes, read_bytes;

/* Process */
PROCESS(example_filesys_lib_process, "Example filesys lib process");
AUTOSTART_PROCESSES(&example_filesys_lib_process);

PROCESS_THREAD(example_filesys_lib_process, ev, data) 
{   
    /* Init */
    write_buffer = 'a';
    
    PROCESS_BEGIN();
    
    /*while (1)
    {*/
       // 1. Writing file
       for (i = 0; i < READ_BUFFER_SIZE; i++)
       {
         printf("write_number = %d\n", i);
         write_bytes = writeFile(&write_buffer, WRITE_BUFFER_SIZE); 
         printf("write_bytes = %d\n\n", write_bytes);
         printf("write_buffer = %c\n", write_buffer);
         write_buffer++;
       }
       
       // 2. Reading file
       read_bytes = readFile(read_buffer, READ_BUFFER_SIZE);
       printf("read_bytes = %d\n\n", read_bytes);
       printf("read_buffer[] = {%c", read_buffer[0]);
       for (i = 1; i < READ_BUFFER_SIZE; i++)
       {
         printf(", %c", read_buffer[i]);
       }
       printf("}\n");
    //}
    
    PROCESS_END(); 
}

