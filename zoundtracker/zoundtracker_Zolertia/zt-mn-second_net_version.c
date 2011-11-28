// File: "zt-mn-second.c"
// Date (last rev.): 31/07/2011
   
#include "contiki.h"
#include "leds.h"
#include "cfs/cfs.h"
#include "net/rime.h"
#include "net/rime/mesh.h"
#include "net/rime/broadcast.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lib/zt-packet-mgmt.h"
#include "dev/i2cmaster.h"
// #include "dev/zt.h"
#include "dev/battery-sensor.h"

// Provisional
#include "dev/z1-phidgets.h"

// File Manager
#include "lib/zt-filesys-lib.h"

// Debug lib
#include "lib/zt-debug-lib.h"

//Net lib
#include "zt-net-lib.h"

//--------------------------------------------------------------------

#ifdef TRUE
#undef TRUE
#endif
#define TRUE 1

#ifdef FALSE
#undef FALSE
#endif
#define FALSE 0

#ifdef DEBUG_MODE
#define DEBUG_NET
#define DEBUG_STATE
#define DEBUG_CFS
#define DEBUG_SENSOR
#define DEBUG_EVENT
#endif

// CFS
#define NUM_SECONDS_SAMPLE 20  // Default 60 (1 sample/min)
#define MAX_WRITE_ATTEMPTS 5

// Sensor
#define SAMPLE_SIZE 4

// State
#define BLOCKED 1
#define DATA_COLLECT 2
#define NET_RECEIVED 3


//--------------------------------------------------------------------

// CFS
static FileManager fmanLocal;
static FileManager fmanNet;
static int write_bytes;
static struct etimer control_timer;
static char initOkLocal;
static char initOkNet;
static int write_attempts;


// State
static unsigned char state;
static int tics;

// Sensor
void 
get_sensor_sample(void)
{
    // Function: This function takes a sample of the ZoundTracker 
    // board. Then stores the pair (number of sample, sample value) 
    // into the Filesystem. By this way we can associate every sample 
    // to every minute on the sample period.

    // Context:  This function is executed when the node enters into 
    // "DATA_COLLECT" state to take a sensor sample.
     	
    if (write_attempts == 0) 
    {
        int32_t t_buffer = 0;
        uint32_t bigassbuffer = 0;
        uint8_t i = 0;

        for (i=0;i<200;i++) 
        {
            t_buffer = phidgets.value(PHIDGET3V_2);
            t_buffer -= 2031;
            bigassbuffer += (uint32_t)(t_buffer * t_buffer);
        }
        
        bigassbuffer /= 200;
        printf("[ZT]\n Raw mean square: %lu\n", bigassbuffer);     
        bigassbuffer = (bigassbuffer >> 1);

        // Building the 'Sample' structure for writing.
        current_sample.number = sample_interval;
        current_sample.value[0] = (char)((bigassbuffer & 0xFF0000)>>16);
        current_sample.value[1] = (char)((bigassbuffer & 0x00FF00)>>8);
        current_sample.value[2] = (char)(bigassbuffer & 0x0000FF);
    }

    write_bytes = write(&fmanLocal, &current_sample,SAMPLE_SIZE);
    if (write_bytes == ERROR_INVALID_FD) 
    {
        ++write_attempts;
        if (write_attempts < MAX_WRITE_ATTEMPTS) get_sensor_sample();
    }
    write_attempts = 0;
}

//--------------------------------------------------------------------

// Process
PROCESS(example_zoundt_mote_process, "Example zoundt mote process");
AUTOSTART_PROCESSES(&example_zoundt_mote_process);

PROCESS_THREAD(example_zoundt_mote_process, ev, data) {   
    
    PROCESS_EXITHANDLER(mesh_close(&zoundtracker_conn);)
    PROCESS_BEGIN();
    
    // NET
    initNet();

    //num_msg_sended = 0;
    //num_msg_acked = 0;
	
    // CFS
    etimer_set(&control_timer, NUM_SECONDS_SAMPLE*CLOCK_SECOND);
    sample_interval = 0;
    initOkLocal = FALSE;
    initOkNet = FALSE;
    initOkLocal = initFileManager(&fmanLocal, 'l', 10, 60);
    initOkNet = initFileManager(&fmanNet, 'n', 3, 60);
    read_attempts = 0;
    write_attempts = 0;
    
    // Sensor
    //zt_init();
    SENSORS_ACTIVATE(phidgets);

    // State
    state = BLOCKED; 
    tics = 0; 

    #ifdef DEBUG_STATE
      debug_state_current_state("BLOCKED");
    #endif


    #ifdef DEBUG_FILEMAN
      debug_filesys_initOk(&fmanLocal,initOkLocal);
      debug_filesys_initOk(&fmanNet,initOkNet);
    #endif

	
    // Sending message ("HELLO_MN"). Changing to "NET_SEND" from 
    // "BLOCKED" state.  
    
    
    // Sending first "HELLO_MN" message.
    //Llamar a funcion send del netManager con el tipo HELLO
    
    while (TRUE)
    {
        PROCESS_WAIT_EVENT();
        if (ev == PROCESS_EVENT_TIMER)
        {
          tics++;
 
          state = NET_RECEIVED;
          #ifdef DEBUG_STATE
                debug_state_current_state("DATA COLLECT");
          #endif
          //comprobar si hay algun paquete de datos en la cola received
          //del modulo de red. En caso afirmativo, almacenar en fmanNet

          state = BLOCKED;
          #ifdef DEBUG_STATE
            debug_state_current_state("BLOCKED");
          #endif

           if(tics == 3)
           {
             state = DATA_COLLECT;
             #ifdef DEBUG_STATE
                debug_state_current_state("DATA COLLECT");
             #endif
                    
             // 'sample_interval' starts on zero.
             get_sensor_sample();
              
             #ifdef DEBUG_SENSOR
               debug_sensor_sample_measured(sample_interval);
             #endif

             //updateNet();

             tics = 0;
          }


        }
        else
        {	
            #ifdef DEBUG_EVENT
              debug_event_current_event("Unknown event");  
            #endif
        }
        
        // Reseting the timer. 
        etimer_set(&control_timer, NUM_SECONDS_SAMPLE*CLOCK_SECOND);
        state = BLOCKED;
        #ifdef DEBUG_STATE
          debug_state_current_state("BLOCKED");
        #endif
    }

    PROCESS_END(); 
}
