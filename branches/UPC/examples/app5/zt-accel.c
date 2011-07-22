#include "contiki.h"
#include "leds.h"
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
#define ACCEL_BUFFER_SIZE 5
#define MAX_WRITES 2
#define MAX_FILES 50
#define MAX_ATTEMPTS 3
#define NUM_SECONDS_WRITE 5

// NET Defines
#define MY_ADDR1 2
#define MY_ADDR2 0
#define SINK_ADDR1 111
#define SINK_ADDR2 111
#define CHANNEL 111
#define NUM_SECONDS_HELLO 120

//------------------------------------------------------------------------------

// CFS variables
static int write_file, send_file, write_num, write_bytes, read_bytes, fd, i, first_message;
static struct etimer write_timer;
static unsigned char filename[FILE_NAME_SIZE];
static unsigned char read_buffer[READ_BUFFER_SIZE];
    
// NET variables
static int attempts;
static struct ctimer hello_timer;
static rimeaddr_t sink_addr;
static struct mesh_conn zoundtracker_conn;

// Sensor variables
static int x, accel_bytes;
static unsigned char accel_buffer[ACCEL_BUFFER_SIZE];

//------------------------------------------------------------------------------

// NET Functions
static void hello_protocol() 
{    
    packetbuf_copyfrom((void *)"hello", 5);
	attempts = 0;
	first_message = 0;			
	sink_addr.u8[0] = SINK_ADDR1;
	sink_addr.u8[1] = SINK_ADDR2;
	mesh_send(&zoundtracker_conn, &sink_addr);
	printf("[net] sending 'hello' message\n\n");

    // Programming next 'hello' message
    ctimer_set(&hello_timer, NUM_SECONDS_HELLO*CLOCK_SECOND, hello_protocol, NULL);
}

static void sent(struct mesh_conn *c) 
{
    if (first_message == 0)
    {
        printf("[net] sent 'hello' message\n\n");
        first_message = 1;
    }
    else
    {
        printf("[net] sent current file\n filename: file%d\n\n", send_file);
        // Updating the current sending file
        send_file++; 
    }
    // 'Error' mode finished
    leds_off(LEDS_RED);
}

static void timedout(struct mesh_conn *c) 
{
    // Resending the current send file
    attempts++;
	if (attempts < MAX_ATTEMPTS) 
	{
	    if (first_message == 0)
	    {
	        printf("[net] timeout resending 'hello' message\n\n");
	        packetbuf_copyfrom((void *)"hello", 5);
	    }
	    else
	    {
	        printf("[net] timeout resending the current file\n filename: file%d\n\n", send_file);
	        packetbuf_copyfrom((void*)read_buffer, read_bytes);
	    }
	    
		sink_addr.u8[0] = SINK_ADDR1;
		sink_addr.u8[1] = SINK_ADDR2;
		mesh_send(&zoundtracker_conn, &sink_addr);
	}
	else
	{
	    printf("[net] maximum number of attempts reached\n message lost\n\n");
        // 'Error' mode started
        leds_on(LEDS_RED);
	}
}

static void received(struct mesh_conn *c, const rimeaddr_t *from, uint8_t hops) 
{
	if (!memcmp(packetbuf_dataptr(), (void *)"poll", 4)) 
	{    
		printf("[net] 'poll' message received\n\n");
		// 'Hello' mode finished
		leds_off(LEDS_GREEN);
		// 'Poll' mode started
		leds_on(LEDS_YELLOW);
		
		
		if (send_file >= write_file)
		{
		    printf("[net] all pending files are sended\n no response\n\n");
		}
		else
		{
	        // Generating the filename of the current sending file
	        sprintf(filename, "file%d", send_file);
	    
	        // Reading the current sending file
	        fd = cfs_open(filename, CFS_READ);
	        if (fd < 0)
	        {
	            printf("[cfs] error openning file\n filename: %s\n\n", filename);
	        }
	        else 
	        {   
	            //printf("[cfs] open: file open successful (%s)\n", filename);        
                
                read_bytes = cfs_read(fd, read_buffer, READ_BUFFER_SIZE);
                if (read_bytes < 1)
	            {
	                printf("[cfs] error reading from the file\n num. bytes readed: %d\n\n", write_bytes);
	            }
	            else
	            {
	                //printf("[cfs] read: file read successful (%d)(%s)\n", read_bytes, read_buffer);
	            
	                printf("[net] sending the current file\n filename: file%d\n\n", send_file);
	                
	                // Sending the current sending file
	                packetbuf_copyfrom((void*)read_buffer, read_bytes);
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
	ctimer_set(&hello_timer, NUM_SECONDS_HELLO*CLOCK_SECOND, hello_protocol, NULL);
	       
    // CFS Initialization
    write_file = send_file = write_num = first_message = 0;
    etimer_set(&write_timer, NUM_SECONDS_WRITE*CLOCK_SECOND);
    
    // Sensor Initialization
    accm_init();
    
    // Sending hello message to basestation
	mesh_open(&zoundtracker_conn, CHANNEL, &zoundtracker_callbacks);
	
	packetbuf_copyfrom((void *)"hello", 5);
	attempts = 0;			
	sink_addr.u8[0] = SINK_ADDR1;
	sink_addr.u8[1] = SINK_ADDR2;
	mesh_send(&zoundtracker_conn, &sink_addr);
	leds_on(LEDS_GREEN);
	printf("[net] sending 'hello' message\n\n");
    
    // Erase all files
    for (i = 0; i < MAX_FILES; i++) 
    {
        sprintf(filename, "file%d", i);
        cfs_remove(filename);
    }
        
    while (1) 
    {
	    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&write_timer));
	    //printf("[timer] time has expired\n");
	    
	    // Reading data from the accelerometer	
	    x = accm_read_axis(X_AXIS);
	    printf("[accel] x axis readed\n value: %d\n\n", x);
	    
		if (write_file >= MAX_FILES)
		{
		    printf("[file] maximum number of files reached\n don't take any measure\n\n");
		} 
		else
		{
		    // Generating the filename of the current writing file
	        sprintf(filename, "file%d", write_file);
	        
	        // Writing data into the current writing file
	        fd = cfs_open(filename, CFS_WRITE | CFS_APPEND);
	        
	        if (fd < 0) 
	        {
	            printf("[cfs] error openning file\n filename: %s\n\n", filename);
	        }
	        else 
	        {
	            //printf("[cfs] open: file open successful (%s)\n", filename);
	            accel_bytes = sprintf(accel_buffer, "%d$", x);
	            write_bytes = cfs_write(fd, accel_buffer, accel_bytes);
	            if (write_bytes != accel_bytes) 
	            {
	                printf("[cfs] write: error writing into the file\n num. bytes writed: %d\n\n", write_bytes);
	            }
	            else 
	            {
	                //printf("[cfs] write: file write successful (%d)\n", write_bytes);
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
	            printf("[file] creating a new file\n\n");
	        }
	    }
	    
        etimer_reset(&write_timer);
        //printf("[timer] time reset\n");
    }
    
    PROCESS_END(); 
}

//------------------------------------------------------------------------------
