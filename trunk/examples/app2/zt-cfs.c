#include "contiki.h"
//#include "dev/leds.h"
#include "cfs/cfs.h"
#include <stdio.h>
#include <string.h>

#define FILE_NAME_SIZE 8
#define READ_BUFFER_SIZE 80
#define MAX_WRITE_NUM 6

PROCESS(example_zoundt_process, "Example ZoundT Process");
AUTOSTART_PROCESSES(&example_zoundt_process);

PROCESS_THREAD(example_zoundt_process, ev, data)
{
    static int write_file, /*send_file,*/ write_num, write_bytes, read_bytes, fd, i;
    static struct etimer write_timer;
    static unsigned char filename[FILE_NAME_SIZE];
    static unsigned char read_buffer[READ_BUFFER_SIZE];
    
    PROCESS_BEGIN();
        
    write_file = /*send_file =*/ write_num = 0;
    etimer_set(&write_timer, 5*CLOCK_SECOND);
    sprintf(read_buffer, "aaaaaa");
    
    // Erase all files
    for (i = 0; i < 20; i++)
    {
        sprintf(filename, "file%d", i);
        cfs_remove(filename);
    }
        
    while (1)
	{
	    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&write_timer));
	    printf("write_timer: time has expired\n");
		    
	    sprintf(filename, "file%d", write_file);
	    
	    // Writing data into the current writing file
	    fd = cfs_open(filename, CFS_WRITE | CFS_APPEND);
	    if (fd < 0)
	    {
	        printf("cfs_open: error openning file (%s)\n", filename);
	    }
	    else 
	    {
	        printf("cfs_open: file open successful (%s)\n", filename);
	        write_bytes = cfs_write(fd,"b",1);
	        if (write_bytes != 1)
	        {
	            printf("cfs_write: error writing into the file (%d)\n", write_bytes);
	        }
	        else
	        {
	            printf("cfs_write: file write successful (%d)\n", write_bytes);
	            cfs_close(fd);
            }
        }
        
        // Reading data from the current writing file
        fd = cfs_open(filename, CFS_READ);
	    if (fd < 0)
	    {
	        printf("cfs_open: error openning file (%s)\n", filename);
	    }
	    else 
	    {   
	        printf("cfs_open: file open successful (%s)\n", filename);        
            read_bytes = cfs_read(fd, read_buffer, READ_BUFFER_SIZE);
            if (read_bytes < 1)
	        {
	            printf("cfs_read: error reading from the file (%d)\n", write_bytes);
	        }
	        else
	        {
	            printf("cfs_read: file read successful (%d)(%s)\n", read_bytes, read_buffer);
	            cfs_close(fd);
            }
	    }
	  
        sprintf(read_buffer, "aaaaaa");
        write_num++;
        
        // Selecting a file to write data
	    if (write_num >= MAX_WRITE_NUM)
	    {
	        // Creating a new file
	        write_file++;
	        printf("file: creating a new file\n");
	        write_num = 0;
	    }
        
        etimer_reset(&write_timer);
        printf("write_timer: time reset\n");
    }
    
    PROCESS_END(); 
}
