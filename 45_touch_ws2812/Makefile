all : flash

TARGET:=touch_ws2812
TARGET_MCU?=CH32V003

CH32FUN?=../ch32fun/ch32fun
# 1 = SOP-16/QFN-20 target board pins. "make clean" after changing
BOARD_TARGET?=0
EXTRA_CFLAGS+=-DBOARD_TARGET=$(BOARD_TARGET)
include $(CH32FUN)/ch32fun.mk

flash : cv_flash
clean : cv_clean
