#include "contiki.h"
#include "cfs/cfs.h"
#include "net/rime.h"
#include "net/rime/mesh.h"
#include "adxl345.h"
#include <stdio.h>
#include <string.h>

//------------------------------------------------------------------------------

// CFS Defines
#define FILE_NAME_SIZE 8
#define READ_BUFFER_SIZE 80
#define MAX_WRITES 6
#define MAX_FILES 20
#define MAX_ATTEMPTS 3
#define NUM_SECONDS 5

// NET Defines
#define MY_ADDR1 1
#define MY_ADDR2 121
#define SINK_ADDR1 111
#define SINK_ADDR2 111
#define CHANNEL 111

//------------------------------------------------------------------------------

// CFS variables
static int write_file, send_file, write_num, write_bytes, read_bytes, fd, i, first_message;
static struct etimer write_timer;
static unsigned char filename[FILE_NAME_SIZE];
static unsigned char read_buffer[READ_BUFFER_SIZE];
    
// NET variables
static int attempts;
static rimeaddr_t sink_addr;
static struct mesh_conn zoundtracker_conn;

// Sensor variables
static int16_t x;

//------------------------------------------------------------------------------

// NET Functions
static void sent(struct mesh_conn *c) 
{
    if (first_message == 1)
    {
        printf("net_sent: sent hello message\n");
        first_message = 0;
    }
    else
    {
        printf("net_sent: current send file sended\n");
    
        // Updating the current sending file
        send_file++; 
    }
}

static void timedout(struct mesh_conn *c) 
{
    // Resending the current send file
    attempts++;
	if (attempts < MAX_ATTEMPTS) 
	{
	    printf("net_timedout: resending the current send file\n");
	    
		sink_addr.u8[0] = SINK_ADDR1;
		sink_addr.u8[1] = SINK_ADDR2;
		mesh_send(&zoundtracker_conn, &sink_addr);
	}
	else
	{
	    printf("net_timedout: maximum number of attempts reached, message lost\n");
	}
}

static void received(struct mesh_conn *c, const rimeaddr_t *from, uint8_t hops) 
{
	if (!strcmp((char *)packetbuf_dataptr(), "poll")) 
	{    
		printf("net_received: poll received\n\n");
		
		if (send_file >= write_file)
		{
		    printf("net_received: all writed files are sended, no response");
		}
		else
		{
	        // Generating the filename of the current sending file
	        sprintf(filename, "file%d", send_file);
	    
	        // Reading the current sending file
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
	            
	                printf("net_received: sending the current send file\n");
	                
	                // Sending the current sending file
	                packetbuf_copyfrom(read_buffer, read_bytes);
	                attempts = 0;			
			        sink_addr.u8[0] = SINK_ADDR1;
			        sink_addr.u8[1] = SINK_ADDR2;
			        mesh_send(&zoundtracker_conn, &sink_addr);     
                }
                
                cfs_close(fd);
	        } 
	    }
	}
}

const static struct mesh_callbacks zoundtracker_callbacks = {received, sent, timedout};

//------------------------------------------------------------------------------

// Process
PROCESS(example_zoundt_mote_process, "Example zoundt mote process");
AUTOSTART_PROCESSES(&example_zoundt_mote_process);

PROCESS_THREAD(example_zoundt_mote_process, ev, data) {   
    
    PROCESS_EXITHANDLER(mesh_close(&zoundtracker_conn);)
    PROCESS_BEGIN();
    
    // NET Initialization
    rimeaddr_t my_addr;
	my_addr.u8[0] = MY_ADDR1;
	my_addr.u8[1] = MY_ADDR2;
	rimeaddr_set_node_addr(&my_addr);
	       
    // CFS Initialization
    write_file = send_file = write_num = 0;
    first_message = 1;
    etimer_set(&write_timer, NUM_SECONDS*CLOCK_SECOND);
    
    // Sensor Initialization
    accm_init();
    
    // Sending hello message to basestation
	mesh_open(&zoundtracker_conn, CHANNEL, &zoundtracker_callbacks);
	
	packetbuf_copyfrom("hello", 5);
	attempts = 0;			
	sink_addr.u8[0] = SINK_ADDR1;
	sink_addr.u8[1] = SINK_ADDR2;
	mesh_send(&zoundtracker_conn, &sink_addr);
    
    
    // Erase all files
    for (i = 0; i < MAX_FILES; i++) 
    {
        sprintf(filename, "file%d", i);
        cfs_remove(filename);
    }
        
    while (1) 
    {
	    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&write_timer));
	    printf("write_timer: time has expired\n");
	    
	    // Reading data from the accelerometer	
	    x = accm_read_axis(X_AXIS);
	    printf("accel: x axis readed (%d)\n", x);
	    
		if (write_file >= MAX_FILES)
		{
		    printf("file: maximum number of files reached\n");
		} 
		else
		{
		    // Generating the filename of the current writing file
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
	            
	            write_bytes = cfs_write(fd, (const void *)x, sizeof(int16_t));
	            if (write_bytes != sizeof(int16_t)) 
	            {
	                printf("cfs_write: error writing into the file (%d)\n", write_bytes);
	            }
	            else 
	            {
	                printf("cfs_write: file write successful (%d)\n", write_bytes);
	                cfs_close(fd);
                }
            }

            write_num++;
            // Selecting a file to write data
	        if (write_num >= MAX_WRITES) 
	        {
	            // Creating a new file
	            write_file++;
	            write_num = 0;
	            printf("file: creating a new file\n");
	        }
	    }
        etimer_reset(&write_timer);
        printf("write_timer: time reset\n");
    }
    
    PROCESS_END(); 
}

//------------------------------------------------------------------------------
