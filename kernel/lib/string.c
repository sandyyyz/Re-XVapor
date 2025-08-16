#include "types.h"

void*
memset(void *dst, int c, uint n)
{
  char *cdst = (char *) dst;
  int i;
  for(i = 0; i < n; i++){
    cdst[i] = c;
  }
  return dst;
}

int
memcmp(const void *v1, const void *v2, uint n)
{
  const uchar *s1, *s2;

  s1 = v1;
  s2 = v2;
  while(n-- > 0){
    if(*s1 != *s2)
      return *s1 - *s2;
    s1++, s2++;
  }

  return 0;
}

// memcpy
void*
memmove(void *dst, const void *src, uint n)
{
  const char *s;
  char *d;

  if(n == 0)
    return dst;
  
  s = src;
  d = dst;
  if(s < d && s + n > d){
    s += n;
    d += n;
    while(n-- > 0)
      *--d = *--s;
  } else
    while(n-- > 0)
      *d++ = *s++;

  return dst;
}

// memcpy exists to placate GCC.  Use memmove.
void*
memcpy(void *dst, const void *src, uint n)
{
  return memmove(dst, src, n);
}

int
strncmp(const char *p, const char *q, uint n)
{
  while(n > 0 && *p && *p == *q)
    n--, p++, q++;
  if(n == 0)
    return 0;
  return (uchar)*p - (uchar)*q;
}

char*
strncpy(char *des, const char *src, int n)
{
  char *os;

  os = des;
  while(n-- > 0 && (*des++ = *src++) != 0)
    ;
  while(n-- > 0)
    *des++ = 0;
  return os;
}

int strcmp(const char *p, const char *q)
{
    while (*p && *p == *q)
        p++, q++;
    return (uchar)*p - (uchar)*q;
}

// Like strncpy but guaranteed to NUL-terminate.
char*
safestrcpy(char *s, const char *t, int n)
{
  char *os;

  os = s;
  if(n <= 0)
    return os;
  while(--n > 0 && (*s++ = *t++) != 0)
    ;
  *s = 0;
  return os;
}

int
strlen(const char *s)
{
  int n;

  for(n = 0; s[n]; n++)
    ;
  return n;
}

size_t strnlen(const char *s, size_t count) {
  const char *sc;

  for (sc = s; *sc != '\0' && count--; ++sc)
      /* nothing */;
  return sc - s;
}

char *
strcpy(char *des, const char *src)
{
    char *os;

    os = des;
    while ((*des++ = *src++) != 0)
        ;
    return os;
}

/**
 * @brief Compare if shorts is a substring of longs
 * @param shorts the substring
 * @param longs the string to be compared
 * @return 0 if shorts is a substring of longs, -1 otherwise
 */
int substr_cmp(const char *shorts, const char *longs) {
  while(*shorts && *shorts == *longs) {
    shorts++;
    longs++;
  }
  if(*shorts == '\0') {
    return 0;
  } else {
    return -1;
  }
}

char *strcat(char *dest, const char *src) {
  char *p = dest;
  while (*p) {
      ++p;
  }
  while (*src) {
      *p++ = *src++;
  }
  *p = '\0';
  return dest;
}
