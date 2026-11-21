/**
 * This file has no copyright assigned and is placed in the Public Domain.
 * This file is part of the TinyCC package.
 * No warranty is given; refer to the file DISCLAIMER within this package.
 */

#ifndef _INC_UCHAR
#define _INC_UCHAR

#if __STDC_VERSION__ >= 202311L
#define __STDC_VERSION_UCHAR_H__ 202311L
#endif

/*
 * The following defines are only valid when C11 (-std=c11) is used.
 *
 * ... a wide character constant prefixed by the letter u or U has type char16_t
 * or char32_t, respectively, unsigned integer types defined in the <uchar.h>
 * header.
 */

#if __STDC_VERSION__ >= 201112L
/**
 * __STDC_UTF_16__ The integer constant 1, intended to indicate that
 * values of type char16_t are UTF-16 encoded.
 */
#define __STDC_UTF_16__ 1
/**
 * __STDC_UTF_32__ The integer constant 1, intended to indicate that
 * values of type char32_t are UTF-32 encoded.
 */
#define __STDC_UTF_32__ 1

#include <stddef.h>
#include <wchar.h>  /* mbstate_t */

typedef unsigned short char16_t;
typedef unsigned int char32_t;
#if __STDC_VERSION__ >= 202311L
typedef unsigned char char8_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif
/* in libtcc1.a (lib/c23uchar.c); the multibyte encoding is the C runtime's
   current locale and characters outside the BMP are not supported there */
size_t __cdecl mbrtoc16(char16_t *__restrict__ _Pc16,const char *__restrict__ _S,size_t _N,mbstate_t *__restrict__ _Ps);
size_t __cdecl c16rtomb(char *__restrict__ _S,char16_t _C16,mbstate_t *__restrict__ _Ps);
size_t __cdecl mbrtoc32(char32_t *__restrict__ _Pc32,const char *__restrict__ _S,size_t _N,mbstate_t *__restrict__ _Ps);
size_t __cdecl c32rtomb(char *__restrict__ _S,char32_t _C32,mbstate_t *__restrict__ _Ps);
#if __STDC_VERSION__ >= 202311L
size_t __cdecl mbrtoc8(char8_t *__restrict__ _Pc8,const char *__restrict__ _S,size_t _N,mbstate_t *__restrict__ _Ps);
size_t __cdecl c8rtomb(char *__restrict__ _S,char8_t _C8,mbstate_t *__restrict__ _Ps);
#endif
#ifdef __cplusplus
}
#endif
#endif /* __STDC_VERSION__ >= 201112L */
#endif /* _INC_UCHAR */
