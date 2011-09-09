ifndef TARGET
TARGET=z1
endif

CONTIKI_PROJECT = zt-mn-second zt-sink zt-mn-final test-zt-phidget
CONTIKI_SOURCEFILES += cc2420-arch.c sensors.c
PROJECT_SOURCEFILES = i2cmaster.c  adxl345.c


all: $(CONTIKI_PROJECT)

CONTIKI = ../..
include $(CONTIKI)/Makefile.include
