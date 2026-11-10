#ifndef _STDARG_H
#define _STDARG_H

typedef __builtin_va_list va_list;
#if __STDC_VERSION__ >= 202311L
/* C23: the second argument is optional and not evaluated */
#define va_start(ap, ...) __builtin_va_start(ap, __builtin_va_last)
#define __STDC_VERSION_STDARG_H__ 202311L
#else
#define va_start __builtin_va_start
#endif
#define va_arg __builtin_va_arg
#define va_copy __builtin_va_copy
#define va_end __builtin_va_end

/* fix a buggy dependency on GCC in libio.h */
typedef va_list __gnuc_va_list;
#define _VA_LIST_DEFINED

#endif /* _STDARG_H */
