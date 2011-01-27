CONTIKI = ../..
ifndef TARGET
TARGET=z1
endif
all: zt-mn zt-sink
CONTIKI_SOURCEFILES += cc2420-arch.c
PROJECT_SOURCEFILES = i2cmaster.c adxl345.c

include $(CONTIKI)/Makefile.include
