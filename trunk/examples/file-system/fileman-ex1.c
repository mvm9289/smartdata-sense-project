#include "contiki.h"
#include "cfs/cfs.h"
#include <stdio.h>
#include <string.h>
#include "lib/zt-filesys-lib.h"

#define READ_BUFFER_SIZE 1
#define WRITE_BUFFER_SIZE 1
#define NUM_SECONDS_WRITE 5
#define STATE_INIT 0
#define STATE_WRITE 1
#define STATE_READ 2

static struct etimer control_timer;
static char read_buffer, write_buffer, initOk;
static int state, write_number, read_number, write_bytes, read_bytes;

/* Process */
PROCESS(example_filesys_lib_process, "Example filesys lib process");
AUTOSTART_PROCESSES(&example_filesys_lib_process);

PROCESS_THREAD(example_filesys_lib_process, ev, data) 
{   
    PROCESS_BEGIN();
    
    /* 0. Init */
    initOk = initFileManager();
    state = write_bytes = read_bytes = 0;
    write_number = read_number = 1;
    write_buffer = 'a';
    etimer_set(&control_timer, NUM_SECONDS_WRITE*CLOCK_SECOND);
    
    while (1)
    {
        PROCESS_WAIT_EVENT();
        if (ev == PROCESS_EVENT_TIMER)
        {
          if (state == STATE_INIT)
          {
            printf("[file-manager]\ninitOk = %d\n\n", (int) initOk);
            
            state = STATE_WRITE;
          }
          else if (state == STATE_WRITE)
          {
            // 1. Writing file
            printf("write_number = %d\n", write_number);
            write_bytes = writeFile(&write_buffer, WRITE_BUFFER_SIZE); 
            printf("write_bytes = %d\n", write_bytes);
            printf("write_buffer = %c\n\n", write_buffer);
            write_buffer++;
            write_number++;
            
            state = STATE_READ;
          }
          else if (state == STATE_READ)
          {
            // 2. Reading file
            printf("read_number = %d\n", read_number);
            read_bytes = readFile(&read_buffer, READ_BUFFER_SIZE);
            printf("read_bytes = %d\n", read_bytes);
            printf("read_buffer = %c\n\n", read_buffer);
            read_number++;
          
            state = STATE_WRITE;
          }
                   
          // 3. Reset timer
          etimer_set(&control_timer, NUM_SECONDS_WRITE*CLOCK_SECOND);  
        }
    }
    
    PROCESS_END(); 
}
