all : flash

TARGET:=i2c_touch
TARGET_MCU?=CH32V003

CH32FUN?=../ch32fun/ch32fun
# 1 = SOP-16/QFN-20 target board pins. "make clean" after changing
BOARD_TARGET?=0
EXTRA_CFLAGS+=-DBOARD_TARGET=$(BOARD_TARGET)
# i2c_slave.h lives with its example, not in extralibs
EXTRA_CFLAGS+=-I$(CH32FUN)/../examples/i2c_slave
include $(CH32FUN)/ch32fun.mk

flash : cv_flash
clean : cv_clean
