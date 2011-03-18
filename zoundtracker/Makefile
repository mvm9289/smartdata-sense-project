CONTIKI = ../..
ifndef TARGET
TARGET=z1
endif
all: zt-mn zt-sink zt-sink-test
CONTIKI_SOURCEFILES += cc2420-arch.c
PROJECT_SOURCEFILES = i2cmaster.c adxl345.c tmp102.c

include $(CONTIKI)/Makefile.include
