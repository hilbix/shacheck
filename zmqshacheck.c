#define	STATIC	static
#include "zmqshacheck.h"

static int
ZMQ_shacheck(void *r, char *line)
{
  char		buf[BUFSIZ];
  size_t	len;

  len	= strlen(line);
  if (len && line[len-1]=='\n')
    line[--len]	= 0;

  ZMQ_out(r, line, len);
  len	= ZMQ_in(r, buf, sizeof buf);
  printf("%s %s\n", buf, line);
  switch (*buf)
    {
    case 'F':
      if (!strncmp("FOUND", buf, 5)) return 0;
    case 'N':
      if (!strncmp("NOTFOUND", buf, 8)) return 1;
    }
  return 2;
}

int
main(int argc, char **argv)
{
  char		buf[BUFSIZ];
  int		ok;
  void		*z, *r;
  const char	*s;

  if (argc<1 || argc>2)
    {
      fprintf(stderr, "Usage: %s [%s] <<<'password'\n", argv[0], SHACHECK_ZMQ_DEFAULT);
      return 1;
    }

  z	= zmq_ctx_new();
  r	= zmq_socket(z, ZMQ_REQ);
  s	= argc==2 ? argv[1] : SHACHECK_ZMQ_DEFAULT;
  ZMQ_connect(r, s);
  ok	= 2;
  while (fgets(buf, (sizeof buf)-1, stdin))
    switch (ZMQ_shacheck(r, buf))
      {
      case 0:
        ok	= 0;
      case 1:
        continue;
      default:
        return 1;
      }
  return ok;
}

