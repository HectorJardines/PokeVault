#ifndef _PRINTF_S
#define _PRINTF_S

#define SMALL_PRINTF 1

#ifdef SMALL_PRINTF
void init_print(void);

int printf(const char *format, ...);

int sprintf(char *out, const char *format, ...);

int snprintf( char *buf, unsigned int count, const char *format, ... );
#endif

#endif /* _PRINTF_S */