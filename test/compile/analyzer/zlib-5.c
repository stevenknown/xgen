/* { dg-additional-options "-O3" } */

#include "analyzer-decls.h"

typedef long unsigned int size_t;
typedef unsigned char Byte;
typedef unsigned int uInt;
typedef unsigned long uLong;

extern size_t strlen(const char *__s);
extern void exit(int __status);
extern char *strcpy(char *restrict __dest, const char *restrict __src);
extern void *calloc(size_t __nmemb, size_t __size);

extern int compress(Byte *dest, uLong *destLen, const Byte *source,
                    uLong sourceLen);

const char hello[] = "hello, hello!";

void test_compress(Byte *compr, uLong comprLen, Byte *uncompr,
                   uLong uncomprLen) {
  int err;
  uLong len = strlen(hello) + 1;

  err = compress(compr, &comprLen, (const Byte *)hello, len);
  if (err != 0)
    exit(1);
  strcpy((char *)uncompr, "garbage"); /* { dg-bogus "NULL" } */
}

int main(int argc, char *argv[]) {
  Byte *compr, *uncompr;
  uLong comprLen = 10000 * sizeof(int);
  uLong uncomprLen = comprLen;

  compr = (Byte *)calloc((uInt)comprLen, 1);
  uncompr = (Byte *)calloc((uInt)uncomprLen, 1);
  if (compr == 0 || uncompr == 0)
    exit(1);

  __analyzer_dump_exploded_nodes (0); /* { dg-warning "1 processed enode" } */

  test_compress(compr, comprLen, uncompr, uncomprLen);

  exit(0);
  return 0;
}
