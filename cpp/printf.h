#pragma once

//Author : richard xu (junzexu@outlook.com)
//Date : Dec 02, 2026

#include <stdarg.h>

//config
#define PRINTF_NTOA_BUFFER_SIZE    32U
#define PRINTF_FTOA_BUFFER_SIZE    32U
#define PRINTF_DEFAULT_FLOAT_PRECISION  6U
#define PRINTF_MAX_FLOAT 1e9

#define FLAGS_ZEROPAD   (1U <<  0U)
#define FLAGS_LEFT      (1U <<  1U)
#define FLAGS_PLUS      (1U <<  2U)
#define FLAGS_SPACE     (1U <<  3U)
#define FLAGS_HASH      (1U <<  4U)
#define FLAGS_UPPERCASE (1U <<  5U)
#define FLAGS_CHAR      (1U <<  6U)
#define FLAGS_SHORT     (1U <<  7U)
#define FLAGS_LONG      (1U <<  8U)
#define FLAGS_LONG_LONG (1U <<  9U)
#define FLAGS_PRECISION (1U << 10U)
#define FLAGS_ADAPT_EXP (1U << 11U)

//types
typedef void (*out_fct_type)(char character, void *buffer, size_t idx, size_t maxlen);

//functions
int vsnprintf_(out_fct_type out, char* buffer, const size_t maxlen, const char* format, va_list va);
int sprintf_(char* buffer, const char* format, ...);
int printf_(const char* format, ...);

#ifdef __cplusplus
extern "C" {
#endif

__declspec(dllexport) void register_internal_putchar(out_fct_type custom_putchar);

#ifdef __cplusplus
}
#endif
