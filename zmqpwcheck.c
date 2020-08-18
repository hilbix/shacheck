#define	STATIC	static
#include "zmqshacheck.h"

#include <openssl/sha.h>

static int
hex(char *buf, size_t max, const void *bin, size_t len)
{
  int	n;
  const unsigned char	*s = bin;

  for (n=0; len>0 && n+2<max; n+=2, len--)
    snprintf(buf+n, 3, "%02x", *s++);
  return n;
}

static int
ZMQ_pwcheck(void *r, char *line)
{
  char		buf[BUFSIZ];
  size_t	len;
  SHA_CTX	h;
  unsigned char	sha[SHA_DIGEST_LENGTH];

  len	= strlen(line);
  if (len && line[len-1]=='\n')
    line[--len]	= 0;

  SHA1_Init(&h);
  SHA1_Update(&h, line, len);
  SHA1_Final(sha, &h);

  len	= hex(buf, sizeof buf, sha, sizeof sha);

  ZMQ_out(r, buf, len);
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
    switch (ZMQ_pwcheck(r, buf))
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

