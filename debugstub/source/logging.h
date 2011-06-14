#ifndef _LOGGING_H_
#define _LOGGING_H_

#include <nds/arm9/console.h>

#ifdef DO_LOGGING

  #include "debug_stub.h"
  /* #define LOG( fmt, ...) logFn_debug( fmt, ##__VA_ARGS__) */
  #define LOG( fmt, ...) iprintf( fmt, ##__VA_ARGS__) 

#else
  #define LOG( fmt, ...)
#endif

/* #define ALWAYS_LOG( fmt, ...) logFn_debug( fmt, ##__VA_ARGS__) */
#define ALWAYS_LOG( fmt, ...) iprintf( fmt, ##__VA_ARGS__) 

#endif
