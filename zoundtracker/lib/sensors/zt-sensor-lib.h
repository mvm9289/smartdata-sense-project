
#ifndef __ZT_SENSOR_LIB_H__
#define __ZT_SENSOR_LIB_H__

#include <stdio.h>



/* Sensor includes */
/*
     Include each sensor files needed in this section
*/

#ifdef TEMP_SENS
#include "dev/i2cmaster.h"
#include "dev/tmp102.h"
#endif

#ifdef BATTERY_SENS
#include "dev/battery-sensor.h"
#endif

#ifdef ACCEL_SENS
#include "adxl345.h"
#endif



/* Sensor types */
/*
     Define a constant for each sensor included in the module in
	 this section.
*/

#define TEMP 0
#define BATTERY 1
#define ACCEL 2



/* Useful typedefs */

typedef unsigned char SensorType;
typedef unsigned char BYTE;



/* Specific sensor data */
/*
     Declare a data structure for each sensor included in the module.
	 Each data structure must contains the necessary data for the
	 sensor. Also can include useful functions related with the sensor
	 (such as getters, setters, auxiliary functions...).
*/

#ifdef TEMP_SENS
typedef struct
{
	int raw;
} TempData;

int getTempInt(TempData *data);
int getTempFrac(TempData *data);
int getSign(TempData *data);
#endif

#ifdef BATTERY_SENS
typedef struct
{
	unsigned int battery;
} BatteryData;
	
float floor(float x);
long getBatteryInt(BatteryData *data);
unsigned getBatteryFrac(BatteryData *data);
#endif

#ifdef ACCEL_SENS
typedef struct
{
	int x_axis;
	int y_axis;
	int z_axis;
} AccelData;

int getX(AccelData *data);
int getY(AccelData *data);
int getZ(AccelData *data);
#endif



/* Global sensor data */
/*
     The global sensor data must contain an union with each different
	 specific sensor data declared. The total size of this struct is
	 the size of SensorType (unsigned char) plus maximum size of
	 specific sensors data included in the union in the build time.
*/

typedef struct
{
	SensorType type;
	union
	{
		#ifdef TEMP_SENS
		TempData temp;
		#endif
		
		#ifdef BATTERY_SENS
		BatteryData battery;
		#endif
		
		#ifdef ACCEL_SENS
		AccelData accel;
		#endif
	} data;
} SensorData;



/* Module functions */
/*
     Global functions to use this module for get data of different
	 sensors, convert data to bytes and bytes to data, and obtain
	 a string representation of data to be understood easily.
*/

void initSensors();
/*
     [Functionality]
        This function initilizes the sensor manager module and each
		sensor used.
     
     [Context]
        This function must be called only one time in the
		initialization section of the process, beyond 'PROCESS_BEGIN()'
        or before to use some functions of the module or a sensor
		declared.
*/
	 
SensorData getSensorData(SensorType type);
/*
     [Functionality]
        This function returns a global sensor data of the sensor type
		defined with the parameter "type" to be processed.
     
     [Context]
        This function is used to get a sample of some sensor type
		defined in the parameter. Before to use this function, the
		module must be initialized. The data returned by this function
		can be used with the rest of module functions for different
		purposes.
*/

void sensorDataToBytes(SensorData* data, BYTE* buffer);
/*
     [Functionality]
        This function convert the sensor data stored in @[data] to a
		byte array stored in @[buffer].
     
     [Context]
        In some cases we need a particular byte representation of some
		data (for instance to send data by the net module), in this
		we can use this function to convert the sensor data to a byte
		array needed for a specific purpose.
*/

void bytesToSensorData(BYTE* buffer, SensorData* data);
/*
     [Functionality]
        This function convert the byte array stored in @[buffer] to a
		sensor data stored in @[data].
     
     [Context]
        When we needed to convert previously a sensor data to a byte
		array, it is usual convert at some time this byte array to
		a sensor data. For this cases we can use this function to do
		this work.
*/

void sensorDataToString(SensorData* data, char* str);
/*
     [Functionality]
        This function gets a string representation of sensor data
		stored in @[data] in the string stored in @[str].
     
     [Context]
        When we want to know the value of a sensor and this value must
		be interpreted by a person, we need a text representation of
		the data. This function returns a like human representation of
		the sensor data in the parameter.
*/

#endif
