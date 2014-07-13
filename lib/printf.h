#ifndef _PRINTF_H
#define _PRINTF_H
#include <lib/stdarg.h>

extern int vsprintf(char * buf, const char * fmt, va_list ap);
extern int sprintf(char * buf, const char * fmt, ...);

#endif
