
#ifndef __ZT_SENSOR_LIB_H__
#define __ZT_SENSOR_LIB_H__

#include <stdio.h>

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
#define TEMP 0
#define BATTERY 1
#define ACCEL 2

typedef unsigned char SensorType;
typedef unsigned char BYTE;

/* Specific sensor data */
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

/* General sensor data */
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

/* Functions */
void initSensors();
SensorData getSensorData(SensorType type);
void sensorDataToBytes(SensorData* data, BYTE* buffer);
void bytesToSensorData(BYTE* buffer, SensorData* data);
void sensorDataToString(SensorData* data, char* str);

#endif
