#include "contiki.h"
#include "cfs/cfs.h"
#include <stdio.h>
#include <string.h>
#include "lib/zt-filesys-lib.h"

#define READ_BUFFER_SIZE 4
#define WRITE_BUFFER_SIZE 4
#define NUM_SECONDS_WRITE 5

#define STATE_INIT 0
#define STATE_WRITE 1
#define STATE_READ 2


static struct etimer control_timer;
static char read_buffer[READ_BUFFER_SIZE + 1], write_buffer[WRITE_BUFFER_SIZE], 
    initOk;
static int state, write_bytes, read_bytes, update;

/* Process */
PROCESS(example_filesys_lib_process, "Example filesys lib process");
AUTOSTART_PROCESSES(&example_filesys_lib_process);

PROCESS_THREAD(example_filesys_lib_process, ev, data) 
{   
    PROCESS_BEGIN();
    
    /* 0. Init */
    initOk = initFileManager();
    state = write_bytes = read_bytes = 0;
    write_buffer[0] = 'a';
    write_buffer[1] = 'b';
    write_buffer[2] = 'c';
    write_buffer[3] = 'd';
    read_buffer[READ_BUFFER_SIZE] = '\0';
    etimer_set(&control_timer, NUM_SECONDS_WRITE*CLOCK_SECOND);
    
    while (1)
    {
        PROCESS_WAIT_EVENT();
        if (ev == PROCESS_EVENT_TIMER)
        {
          if (state == STATE_INIT)
          {
            state = STATE_WRITE;
          }
          else if (state == STATE_WRITE)
          {
            // 1. Writing file
            write_bytes = write(&write_buffer, WRITE_BUFFER_SIZE); 
            
            state = STATE_READ;
          }
          else if (state == STATE_READ)
          {
            // 2. Reading file
            read_bytes = read(&read_buffer, READ_BUFFER_SIZE);
 
            printf("read_buffer = %s\n\n", read_buffer);
                       
            if (read_bytes == 0) {
              update = updateReadFile();
            }
            
            state = STATE_WRITE;           
          }
                   
          // 3. Reset timer
          etimer_set(&control_timer, NUM_SECONDS_WRITE*CLOCK_SECOND);  
        }
    }
    
    PROCESS_END(); 
}
