#ifndef _FUNCONFIG_H
#define _FUNCONFIG_H

// printf goes out SWIO (PD1) to minichlink, view with "make monitor"
// if programmer can't do that, use UART TX on PD5 @ 115200 instead:
// #define FUNCONF_USE_DEBUGPRINTF 0
// #define FUNCONF_USE_UARTPRINTF 1
#define FUNCONF_USE_DEBUGPRINTF 1

#endif
