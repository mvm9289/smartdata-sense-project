
#include "zt-sensor-lib.h"

#ifdef TEMP_SENS
int getTempInt(TempData *data)
{
	unsigned int absraw = data->raw;
	int sign = 1;
	if (data->raw < 0) {
	  absraw = (data->raw ^ 0xFFFF) + 1;
	  sign = -1;
	}
	
	return (absraw >> 8) * sign;
}

int getTempFrac(TempData *data)
{
	unsigned int absraw = data->raw;
	if (data->raw < 0) absraw = (data->raw ^ 0xFFFF) + 1;
	
	return ((absraw>>4) % 16) * 625;
}

int getSign(TempData* data)
{
	if (data->raw < 0) return -1;
	return 1;
}
#endif

#ifdef BATTERY_SENS
float floor(float x)
{
	if(x>=0.0f) return (float)((int)x);
	else return(float)((int)x-1);
}

long getBatteryInt(BatteryData *data)
{
	float mv = (data->battery*2.500*2)/4096;
	return (long)mv;
}

unsigned getBatteryFrac(BatteryData *data)
{
	float mv = (data->battery*2.500*2)/4096;
	return (unsigned)((mv - floor(mv))*1000);
}
#endif

#ifdef ACCEL_SENS
int getX(AccelData *data)
{
	return data->x_axis;
}

int getY(AccelData *data)
{
	return data->y_axis;
}

int getZ(AccelData *data)
{
	return data->z_axis;
}
#endif

/* Functions */
void initSensors()
{
	#ifdef TEMP_SENS
	tmp102_init();
	#endif
	
	#ifdef BATTERY_SENS
	SENSORS_ACTIVATE(battery_sensor);
	#endif
	
	#ifdef ACCEL_SENS
	accm_init();
	#endif
}

SensorData getSensorData(SensorType type)
{
	SensorData data;
	
	data.type = type;
	
	#ifdef TEMP_SENS
	if (type == TEMP)
		data.data.temp.raw = tmp102_read_temp_raw();
	#endif
	
	#ifdef BATTERY_SENS
	if (type == BATTERY)
		data.data.battery.battery = battery_sensor.value(0);
	#endif
	
	#ifdef ACCEL_SENS
	if (type == ACCEL)
	{
		data.data.accel.x_axis = accm_read_axis(X_AXIS);
		data.data.accel.y_axis = accm_read_axis(Y_AXIS);
		data.data.accel.z_axis = accm_read_axis(Z_AXIS);
	}
	#endif
	
	return data;
}

void sensorDataToBytes(SensorData* data, BYTE* buffer)
{
	/*BYTE *ptr = buffer;
	
	((SensorType *)ptr)[0] = data->type;
	ptr += sizeof(SensorType);
	
	#ifdef TEMP_SENS
	if (data->type == TEMP)
		((int*)ptr)[0] = data->data.temp.raw;
	#endif
	
	#ifdef BATTERY_SENS
	if (data->type == BATTERY)
		((unsigned int*)ptr)[0] = data->data.battery.battery;
	#endif
	
	#ifdef ACCEL_SENS
	if (data->type == ACCEL)
	{
		((int*)ptr)[0] = data->data.accel.x_axis;
		((int*)ptr)[1] = data->data.accel.y_axis;
		((int*)ptr)[2] = data->data.accel.z_axis;
	}
	#endif*/
    
    memcpy(buffer,data,sizeof(SensorData));
}

void bytesToSensorData(BYTE* buffer, SensorData* data)
{
	/*BYTE *ptr = buffer;
	
	data->type = ((SensorType *)ptr)[0];
	ptr += sizeof(SensorType);
	
	#ifdef TEMP_SENS
	if (data->type == TEMP)
		data->data.temp.raw = ((int*)ptr)[0];
	#endif
	
	#ifdef BATTERY_SENS
	if (data->type == BATTERY)
		data->data.battery.battery = ((unsigned int*)ptr)[0] ;
	#endif
	
	#ifdef ACCEL_SENS
	if (data->type == ACCEL)
	{
		data->data.accel.x_axis = ((int*)ptr)[0];
		data->data.accel.y_axis = ((int*)ptr)[1];
		data->data.accel.z_axis = ((int*)ptr)[2];
	}
	#endif*/
	
    memcpy(data,buffer,sizeof(SensorData));
}

void sensorDataToString(SensorData* data, char* str)
{
	#ifdef TEMP_SENS
	if (data->type == TEMP)
	{
		int sign = getSign(&(data->data.temp));
		int tempInt = getTempInt(&(data->data.temp));
		unsigned int tempFrac = getTempFrac(&(data->data.temp));
		char minus = ((tempInt == 0) & (sign == -1)) ? '-'  : ' ' ;
		sprintf(str, "%c%d.%04d C", minus, tempInt, tempFrac);
	}
	#endif
	
	#ifdef BATTERY_SENS
	if (data->type == BATTERY)
	{
		long batteryInt = getBatteryInt(&(data->data.battery));
		unsigned batteryFrac = getBatteryFrac(&(data->data.battery));
		sprintf(str, "%ld.%03d mV", batteryInt, batteryFrac);
	}
	#endif
	
	#ifdef ACCEL_SENS
	if (data->type == ACCEL)
	{
		int x = data->data.accel.x_axis;
		int y = data->data.accel.y_axis;
		int z = data->data.accel.z_axis;
		sprintf(str, "(%d, %d, %d)", x, y, z);
	}
	#endif
}
