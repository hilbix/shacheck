/* This Works is placed under the terms of the Copyright Less License,
 * see file COPYRIGHT.CLL.  USE AT OWN RISK, ABSOLUTELY NO WARRANTY.
 */

#define	_GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <unistd.h>

#define	SHACHECK_FILE_VERSION	"0"

#if 1
#define	DP(X)	do { _d_file_=__FILE__; _d_line_=__LINE__; _d_function_=__FUNCTION__; _d_printf_ X; } while (0)
static const char *_d_file_, *_d_function_;
static int _d_line_;
static void
_d_printf_(const char *s, ...)
{
  va_list	list;

  fprintf(stderr, "[[%s:%d: %s", _d_file_, _d_line_, _d_function_);
  va_start(list, s);
  vfprintf(stderr, s, list);
  va_end(list);
  fprintf(stderr, "]]\n");
}
#else
#define	DP(X)	do { ; } while (0)
#endif
#define	xDP(x)

static char	*dir;
static int	err, hashlen;

static void
OOPS(const char *s, ...)
{
  int		e = errno;
  va_list	list;

  fprintf(stderr, "OOPS: ");
  va_start(list, s);
  vfprintf(stderr, s, list);
  va_end(list);
  if (e)
    fprintf(stderr, ": %s", strerror(e));
  fprintf(stderr, "\n");
  exit(23);
}

static void
WARN(int e, const char *s, ...)
{
  va_list	list;

  fprintf(stderr, "WARN: ");
  va_start(list, s);
  vfprintf(stderr, s, list);
  va_end(list);
  if (e)
    fprintf(stderr, ": %s", strerror(e));
  fprintf(stderr, "\n");

  err	= 1;
}

static void *
re_alloc(void *ptr, size_t len)
{
  ptr = realloc(ptr, len);
  if (!ptr)
    OOPS("out of memory");
  return ptr;
}

static void
my_snprintf(char *buf, size_t max, const char *format, ...)
{
  va_list	list;
  int		len;

  va_start(list, format);
  len	= vsnprintf(buf, max, format, list);
  va_end(list);
  if (len>=max)
    OOPS("internal error, buffer overrun: %s", format);
}

static int
isdir(const char *dir)
{
  struct stat	st;

  return !stat(dir, &st) && S_ISDIR(st.st_mode);
}

static int
hexdigit(char c)
{
  if (c>='0' && c<='9')
    return c-'0';
  if (c>='A' && c<='F')
    return c-'A'+10;
  if (c>='a' && c<='f')
    return c-'a'+10;
  return -1;
}

static int
hexbyte(const char *ptr)
{
  int	v;

  v	= hexdigit(*ptr++);
  if (v<0)
    return v;

  v	<<= 4;
  v	|=  hexdigit(*ptr);
  
  return v;
}

static char *
trims(char *s)
{
  int	i;

  while (isspace(*s))
    s++;
  i	= strlen(s);
  while (i>0 && isspace(s[--i]))
    s[i]	= 0;
  return s;
}

#define	MAXINPUT	1000	/* should be enough	*/
#define	MAXHASH		64

struct inputs
  {
    FILE		*fd;
    int			len, eof, garbage, line;
    unsigned char	buf[MAXHASH];
    char		*name;
  } inputs[MAXINPUT];		/* no malloc for now, assume kernel does not map NUL pages	*/

static void
input_ungetc(int c, struct inputs *input)
{
  if (c != ungetc(c, input->fd))
    WARN(errno, "%s:%d: cannot rewind malformed line (missing 2nd hex digit)", input->name, input->line);
}

static int
input_hex(struct inputs *input)
{
  int	c, d, v;

  c	= fgetc(input->fd);
  if (c==EOF)
    return c;

  v	= hexdigit(c);
  if (v<0 || (d = fgetc(input->fd))==EOF)
    {
      input_ungetc(c, input);
      return v;
    }

  /* this should not happen	*/
  v	<<= 4;
  v	|= hexdigit(d);
  if (v<0)
    {
      input_ungetc(d, input);
      input_ungetc(c, input);
    }
  return v;
}

static void
input_read(int file)
{
  struct inputs	*input = &inputs[file];
  FILE		*fd = input->fd;
  int		i, c;

  if (input->eof)
    return;
  for (;;)
    {
      input->line++;

      for (i=0; i<MAXHASH && (c=input_hex(input))>=0; i++)
        input->buf[i]	= c;
      input->len	= i;
    
      c = fgetc(fd);

      /* regular	*/
      if (c=='\n')
	return;
      if (c==EOF)
        { 
          input->eof	= 1;
          return;
        }

      if (!(input->garbage & 1))
	{
	  input->garbage |= 1;
	  WARN(0, "%s:%d: garbage at the EOL\n", input->name, input->line);
        }
      while ((c=fgetc(fd))!='\n')
	if (c==EOF)
	  {
	    input->eof	= 1;
	    return;
          }
      if (i)
        return;

      if (input->garbage & 2)
        continue;

      input->garbage |= 2;
      WARN(0, "%s:%d: line ignored\n", input->name, input->line);
    }
}

/* find the minimum input	*/
static int
input_best(int min, int max)
{
  int	i, a, b;

  xDP(("(%d,%d)", min, max));
  if (min >= max)
    return -1;
  if (min+1 == max)
    {
      for (; !inputs[min].eof; input_read(min))
#if 0
        if (memcmp(inputs[min].buf, last, hashlen))
#endif
          return min;
      return -1;
    }

  i	= (max+min)/2;
  a	= input_best(min, i);
  xDP(("() a=%d", a));
  b	= input_best(i, max);
  xDP(("() b=%d", a));
  return  a<0 ? b : b<0 ? a : memcmp(inputs[a].buf, inputs[b].buf, hashlen)<0 ? a : b;
}

static int
inputs_open(char **argv)
{
  int	files;

  for (files=0; files<MAXINPUT && *argv; argv++, files++)
    {
      inputs[files].name = *argv;
      if ((inputs[files].fd = fopen(*argv, "rt"))==NULL)
        OOPS("%s: cannot read", *argv);
      input_read(files);
    }
  if (*argv)
    OOPS("too many arguments, max %d files possible", MAXINPUT);
  if (!files)
    {
      inputs[files].name = "(stdin)";
      inputs[files].fd	= stdin;
      input_read(files++);
    }
  return files;
}

#define	MAGICSZ	16

static char *
get_magic(char magic[MAGICSZ])
{
  my_snprintf(magic, MAGICSZ, "shaCheck=%s=", SHACHECK_FILE_VERSION);
  return magic;
}

static void
create(char **argv)
{
  FILE		*fd;
  unsigned char	a, b;
  int		files;
  char		tmp[FILENAME_MAX], magic[MAGICSZ];

  files	= inputs_open(argv);

  fd	= 0;
  for (;;)
    {
      struct inputs	*input;
      int		best;

      best	= input_best(0, files);
      xDP(("() best=%d", best));
      if (best<0)
	break;

      input	= &inputs[best];
      if (!hashlen)
        {
	  hashlen	= input->len;
          if (hashlen<8 || hashlen>255)
	    OOPS("%s:%d: HASH length must be in the range of 16 to 512 hex digits: %d", input->name, input->line, hashlen*2);
        }
      else if (hashlen != input->len)
	OOPS("%s:%d: all files must have the same HASH length: %d", input->name, input->line, hashlen);

      if (!fd || a!=input->buf[0] || b!=input->buf[1])
        {
          if (fd && (ferror(fd) || fclose(fd)))
	    OOPS("%s: write error", tmp);
          a	= input->buf[0];
          b	= input->buf[1];
	  my_snprintf(tmp, sizeof tmp, "%s/%02x", dir, a);
          if (!isdir(tmp) && mkdir(tmp, 0755))
	    OOPS("%s: cannot create directory", tmp);
	  my_snprintf(tmp, sizeof tmp, "%s/%02x/%02x.hash", dir, a, b);
	  if ((fd = fopen(tmp, "wb")) == NULL)
	    OOPS("%s: cannot create file", tmp);

	  /* initialize file	*/
          fprintf(fd, "%s%c", get_magic(magic), hashlen);

	  printf("\r%02x%02x", a, b); fflush(stdout);
        }
      fwrite(input->buf+2, hashlen-2, 1, fd);
      input_read(best);
    }
  if (fd && (ferror(fd) || fclose(fd)))
    OOPS("%s: write error", tmp);
}

static void
check_one(char *line)
{
  unsigned char	hash[MAXHASH];
  FILE		*fd;
  int		i, c;
  char		tmp[FILENAME_MAX], buf[BUFSIZ], magic[MAGICSZ];
  long		offset, total;	/* we assume filesize is below 2 GB	*/

  xDP(("(%s)", line));
  line	= trims(line);
  if (!*line)
    return;

  for (i=0; i<MAXHASH && (c=hexbyte(line+i+i))>=0; i++)
    hash[i]	= c;

  xDP(("() %d", i));

  if (line[i+i] && !isspace(line[i+i]))
    {
      WARN(0, "malformed: %s", line);
      return;
    }
  if (i<8 || i>255)
    {
      WARN(0, "hashlength %d not in [8..255]: %s", i, line);
      return;
    }

  my_snprintf(tmp, sizeof tmp, "%s/%02x/%02x.hash", dir, hash[0], hash[1]);
  if ((fd = fopen(tmp, "rb")) == NULL)
    {
      WARN(0, "%s not found: %s", tmp, line);
      return;
    }
  offset	= strlen(get_magic(magic))+1;
  if (1 != fread(buf, offset, 1, fd))
    OOPS("%s: corrupt file, cannot read magic", tmp);
  if (memcmp(buf, magic, offset-1))
    OOPS("%s: corrupt file, wrong magic", tmp);

  c	= (unsigned char)buf[offset-1];
  if (!hashlen)
    hashlen = c>=8 && c<=255 ? c : i;	/* do something meaningful in next OOPS	*/
  if (c != hashlen)
    OOPS("%s: corrupt file, hashlen %d (expected %d)", tmp, c, hashlen);

  if (i != hashlen)
    {
      WARN(0, "wrong hashlength %d (expected %d): %s", i, hashlen, line);
      return;
    }

  /* everything looks good so far	*/

  if (fseek(fd, 0, SEEK_END))
    OOPS("%s: cannot seek to EOF", tmp);

  total	= ftell(fd);
  if ((total - offset) % (hashlen-2))
    OOPS("%s: corrupt file, %ld not divisible by %d", tmp, (total - offset), hashlen-2);

  000;	/* binsearch here	*/

  fclose(fd);
}

static void
check(char **argv)
{
  char	buf[BUFSIZ];

  err = 2;
  if (!*argv)
    while (fgets(buf, (sizeof buf)-1, stdin))
      check_one(buf);
  else
    while (*argv)
      check_one(*argv++);
}

static void
usage(char **argv)
{
  OOPS("Usage: %s datadir create|check args..", argv[0]);
}

int
main(int argc, char **argv)
{
  const char	*cmd;

  if (argc<3)
    usage(argv);

  dir	= argv[1];
  cmd	= argv[2];

  if (!isdir(dir))
    OOPS("%s: not a directory", dir);

  if (!strcmp(cmd, "create"))
    create(argv+3);
  else if (!strcmp(cmd, "check"))
    check(argv+3);
  else
    usage(argv);

  return err;
}

