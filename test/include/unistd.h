#include "stddef.h"

#ifndef _UNISTD_H_
#define _UNISTD_H_

char *getpass(const char *prompt);
int execvp(const char *file, char *const argv[]);

#endif
