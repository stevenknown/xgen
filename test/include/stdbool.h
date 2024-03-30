#ifndef _STDBOOL_H_
#define _STDBOOL_H_

/* All the definitions below are implementation-specific.  */

/* Thestdbool.h header shall define _Bool and the constants true and
   false.  If the implementation provides the stdbool.h header, it
   shall define these macros in the std namespace.  */

#if defined(__STDBOOL_H) || (defined (__cplusplus) && defined (bool))
  /* C++ has a predefined bool type, so we don't define anything.  */
#else
  #if (defined (__STDC_VERSION__) && __STDC_VERSION__ >= 199901L) \
       || (defined (_WIN32) && !defined (__STDC__))
    /* C99 and later, or Windows: use the standard definitions.  */
    #define bool _Bool
    #define true 1
    #define false 0
  #else
    /* For other environments, define our own.  */
    typedef int bool;
    #define true 1
    #define false 0
    #define _Bool bool
  #endif
#endif

#endif
