#define	_GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>

#ifndef	STATIC
#define	STATIC
#endif

#ifndef __SHACHECK_OOPS__
#define	__SHACHECK_OOPS__

STATIC void                                                                                                            
VOOPS(int e, const char *s, va_list list)
{
  fprintf(stderr, "OOPS: ");
  vfprintf(stderr, s, list);
  if (e)
    fprintf(stderr, ": %s", strerror(e));
  fprintf(stderr, "\n");
  exit(23);
}

STATIC void
OOPS(const char *s, ...)
{
  va_list	list;
  int		e = errno;

  va_start(list, s);
  VOOPS(e, s, list);
  va_end(list);
}

#endif

