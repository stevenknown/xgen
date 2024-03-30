#include "stddef.h"

#ifndef _STDLIB_H_
#define _STDLIB_H_

void * calloc(int);
extern void abort();
extern void exit(int);
void * malloc(int);
void free(void*);
unsigned long strtoul(const char *str, char **endptr, int base);
double strtold(const char *str, char **endptr);
void *realloc(void *ptr, size_t new_size);
int mkstemp(char *template);
void _exit(int status);
int atexit(void (*func)(void));
void *calloc(size_t num, size_t size);

#endif
