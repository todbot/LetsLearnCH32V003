all : flash

TARGET:=touchwheel
TARGET_MCU?=CH32V003

CH32FUN?=../ch32fun/ch32fun
# i2c_slave.h lives with its example, not in extralibs
EXTRA_CFLAGS+=-I$(CH32FUN)/../examples/i2c_slave
include $(CH32FUN)/ch32fun.mk

flash : cv_flash
clean : cv_clean
