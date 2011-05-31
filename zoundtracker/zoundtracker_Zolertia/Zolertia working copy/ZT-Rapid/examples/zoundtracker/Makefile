ifndef TARGET
TARGET=z1
endif

CONTIKI_PROJECT = zt-mn zt-sink zt-mn-ziglet
CONTIKI_SOURCEFILES += cc2420-arch.c sensors.c
PROJECT_SOURCEFILES = i2cmaster.c  adxl345.c


all: $(CONTIKI_PROJECT)

CONTIKI = ../..
include $(CONTIKI)/Makefile.include
