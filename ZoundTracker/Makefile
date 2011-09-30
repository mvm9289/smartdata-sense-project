ifndef TARGET
TARGET=z1
endif

CONTIKI_PROJECT = zt-mn zt-sink
CONTIKI_SOURCEFILES += cc2420-arch.c sensors.c
PROJECT_SOURCEFILES = i2cmaster.c adxl345.c tmp102.c battery-sensor.c

all: $(CONTIKI_PROJECT)

CONTIKI = ../../contiki-2.x
include $(CONTIKI)/Makefile.include
