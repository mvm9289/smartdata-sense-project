CONTIKI = ../..
ifndef TARGET
TARGET=z1
endif
all: zoundtracker_sink zoundtracker
CONTIKI_SOURCEFILES += cc2420-arch.c
PROJECT_SOURCEFILES = i2cmaster.c adxl345.c
APPS=serial-shell

include $(CONTIKI)/Makefile.include
