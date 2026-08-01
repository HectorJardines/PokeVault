#ifndef _PRINTF_S
#define _PRINTF_S

int printf(const char *format, ...);

int sprintf(char *out, const char *format, ...);

int snprintf( char *buf, unsigned int count, const char *format, ... );

#endif /* _PRINTF_S */