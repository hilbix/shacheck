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

#if 0
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

#define	SHACHECK_VARIANT_DEF	SHACHECK_VARIANT2
#define	SHACHECK_VARIANT_MAX	SHACHECK_VARIANT3

#define	SHACHECK_MAXINPUT	1000		/* should be enough	*/
#define	SHACHECK_MAGIC		"shaCheck=%d="	/* plus 1 byte hashlength	*/
#define	SHACHECK_MAGIC_BUF	16		/* actually we only need 12 bytes	*/
#define	SHACHECK_MINHASH	8		/* actually it is shacheck.variant+1, but I do not think shorter HASHes are good	*/
#define	SHACHECK_MAXHASH	64
#define	SHACHECK_PATH_NEEDED	14		/* /HHH/HHH.hash	*/

enum shacheck_variant
  {
    SHACHECK_VARIANT2	= 2,
    SHACHECK_VARIANT3	= 3,
  };


struct shacheck_input
  {
    FILE		*fd;
    int			len, eof, garbage, line;
    unsigned char	buf[SHACHECK_MAXHASH];
    char		*name;
  };

struct shacheck
  {
    /* configuration	*/
    enum shacheck_variant	variant;			/* 2:DIR/HH/HH.hash, 3:DIR/HHH/HHH.hash	*/
    char			dir[FILENAME_MAX];		/* DIR where files are stored	*/

    /* state	*/
    int				err;				/* error seen (for return value)?	*/
    int				hashlen;			/* current hash length we operate on	*/

    /* access to .hash-files	*/
    FILE			*fd;				/* currently open hash	*/
    char			hashname[FILENAME_MAX];		/* path to current .hash file	*/
    char			magic[SHACHECK_MAGIC_BUF];	/* file header magic	*/
    long			hash_offset;			/* where the SHAs start	*/
    long			hash_filesize;			/* we assume filesize is below 2 GB	*/

    /* inputs for "create" */
    struct shacheck_input	inputs[SHACHECK_MAXINPUT];	/* no malloc for now, assume kernel does not map NUL pages	*/
    struct shacheck_input	*input;				/* current input	*/
    int				input_count;			/* inputs[0..input_count-1] are used	*/
  };

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
WARN(struct shacheck *m, int e, const char *s, ...)
{
  va_list	list;

  fprintf(stderr, "WARN: ");
  va_start(list, s);
  vfprintf(stderr, s, list);
  va_end(list);
  if (e)
    fprintf(stderr, ": %s", strerror(e));
  fprintf(stderr, "\n");

  m->err	= 1;
}

#if 0
static void *
re_alloc(void *ptr, size_t len)
{
  ptr = realloc(ptr, len);
  if (!ptr)
    OOPS("out of memory");
  return ptr;
}
#endif

static char *
my_snprintf(char *buf, size_t max, const char *format, ...)
{
  va_list	list;
  int		len;

  va_start(list, format);
  len	= vsnprintf(buf, max, format, list);
  va_end(list);
  if (len>=max)
    OOPS("internal error, buffer overrun: %s", format);
  return buf;
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

static void
shacheck_ungetc(struct shacheck *m, int c)
{
  if (c != ungetc(c, m->input->fd))
    WARN(m, errno, "%s:%d: cannot rewind malformed line (missing 2nd hex digit)", m->input->name, m->input->line);
}

static int
shacheck_gethex(struct shacheck *m)
{
  int	c, d, v;

  c	= fgetc(m->input->fd);
  if (c==EOF)
    return c;

  v	= hexdigit(c);
  if (v<0 || (d = fgetc(m->input->fd))==EOF)
    {
      shacheck_ungetc(m, c);
      return v;
    }

  /* this should not happen	*/
  v	<<= 4;
  v	|= hexdigit(d);
  if (v<0)
    {
      shacheck_ungetc(m, d);
      shacheck_ungetc(m, c);
    }
  return v;
}

static struct shacheck_input *
shacheck_input(struct shacheck *m, int input)
{
  if (input<0 || input>=SHACHECK_MAXINPUT)
    OOPS("internal error: input number out of range: %d", input);

  m->input = &m->inputs[input];
  if (!m->input->name)
    OOPS("internal error: unknown input %d", input);
  if (!m->input->fd)
    OOPS("internal error: input %d not open: %s", input, m->input->name);

  return m->input;
}

static void
shacheck_input_read(struct shacheck *m, int file)
{
  int	i, c;

  shacheck_input(m, file);
  if (m->input->eof)
    return;
  for (;;)
    {
      m->input->line++;

      for (i=0; i<SHACHECK_MAXHASH && (c=shacheck_gethex(m))>=0; i++)
        m->input->buf[i]	= c;
      m->input->len	= i;
      if (!m->hashlen && i)
        {
          m->hashlen	= i;
          if (m->hashlen<SHACHECK_MINHASH)
            OOPS("%s:%d: HASH length must be in the range of %d to %d hex digits: %d", m->input->name, m->input->line, SHACHECK_MINHASH*2, SHACHECK_MAXHASH*2, m->hashlen*2);
        }

      c = fgetc(m->input->fd);

      /* regular, new	*/
      if (c==':')
        {
          m->input->garbage |= 4;
          while ((c = getc(m->input->fd)) && c>='0' && c<='9');
        }
      else if ((m->input->garbage&12)==4)
        {
          m->input->garbage |= 8;
          WARN(m, 0, "%s:%d: not in proper new format\n", m->input->name, m->input->line);
        }

      if (c=='\r')
        c = fgetc(m->input->fd);

      /* regular	*/
      if (c=='\n')
        return;
      if (c==EOF)
        { 
          m->input->eof	= 1;
          return;
        }

      if (!(m->input->garbage & 1))
        {
          m->input->garbage |= 1;
          WARN(m, 0, "%s:%d: garbage at the EOL: '%c'\n", m->input->name, m->input->line, c);
        }
      while ((c=fgetc(m->input->fd))!='\n')
        if (c==EOF)
          {
            m->input->eof	= 1;
            return;
          }
      if (i)
        return;

      if (m->input->garbage & 2)
        continue;

      m->input->garbage |= 2;
      WARN(m, 0, "%s:%d: line ignored\n", m->input->name, m->input->line);
    }
}

/* find the input with the lowest sha	*/
static int
shacheck_input_best(struct shacheck *m, int min, int max)
{
  int	i, a, b;

  xDP(("(%d,%d)", min, max));
  if (min >= max)
    return -1;
  if (min+1 == max)
    {
      for (; !m->inputs[min].eof; shacheck_input_read(m, min))
#if 0
        if (memcmp(inputs[min].buf, last, hashlen))
#endif
          return min;
      return -1;
    }

  i	= (max+min)/2;
  a	= shacheck_input_best(m, min, i);
  xDP(("() a=%d", a));
  b	= shacheck_input_best(m, i, max);
  xDP(("() b=%d", a));
  return  a<0 ? b : b<0 ? a : memcmp(m->inputs[a].buf, m->inputs[b].buf, m->hashlen)<0 ? a : b;
}

static void
shacheck_inputs_open(struct shacheck *m, char **argv)
{
  for (m->input_count=0; m->input_count<SHACHECK_MAXINPUT && *argv; argv++, m->input_count++)
    {
      m->inputs[m->input_count].name = *argv;
      if ((m->inputs[m->input_count].fd = fopen(*argv, "rt"))==NULL)
        OOPS("%s: cannot read", *argv);
      shacheck_input_read(m, m->input_count);
    }
  if (*argv)
    OOPS("too many arguments, max %d files possible", SHACHECK_MAXINPUT);
  if (!m->input_count)
    {
      m->inputs[m->input_count].name = "(stdin)";
      m->inputs[m->input_count].fd	= stdin;
      shacheck_input_read(m, m->input_count++);
    }
}

/* structure of the hash name files are:
 * DIR/HH/HH.hash (variant 2)
 * DIR/HHH/HHH.hash (variant 3)
 * inside of the files there is a header followed by a list of binary SHAs
 * with the first 3 byte removed.
 *
 * HHH gives 16^3 = 2^(4*3) = 2^12 = 4096 entries, which is OK
 * HHH/HHH gives us 16 Million files with a rough overhead of 3 GB on ext3.
 *
 * haveibeenpwned.com reports roughly half a billion SHAs (551509767)
 * google reports 4 billion passwords.
 *
 * As soon as we reach ~3 billion passwords, variant 3 is better than variant 2.
 */
static char *
shacheck_mk_hashname(struct shacheck *m, unsigned char *hash, int filename)
{
  int a, b;
  const char *dir, *file;

  switch (m->variant)
    {
    case SHACHECK_VARIANT2:
      a		= hash[0];
      b		= hash[1];
      dir	= "%s/%02x";
      file	= "%s/%02x/%02x.hash";
      break;

    case SHACHECK_VARIANT3:
      a		= ((((unsigned)hash[0])<<4)&0xf00) | ((hash[1]>>4)&0xf);
      b		= ((((unsigned)hash[1])<<8)&0xf00) | ((hash[2]>>0)&0xff);
      dir	= "%s/%03x";
      file	= "%s/%03x/%03x.hash";
      break;

    default:
      OOPS("internal error, unknown shacheck_variant %d", m->variant);
    }

  return my_snprintf(m->hashname, sizeof m->hashname, filename ? file : dir, m->dir, a, b);
}

static void
shacheck_mk_magic(struct shacheck *m)
{
  if (!m->magic[0])
    snprintf(m->magic, sizeof m->magic, SHACHECK_MAGIC, m->variant);
  if (m->magic[sizeof m->magic-1])
    OOPS("internal error creating magic of size %d: %s", (int)sizeof m->magic, SHACHECK_MAGIC);

  m->hash_offset	= strlen(m->magic)+1;	/* == sizeof magic	*/
}

static void
shacheck_hash_close(struct shacheck *m)
{
  if (!m->fd)
    return;
  if (ferror(m->fd) || fclose(m->fd))
    OOPS("%s: write error", m->hashname);
  m->fd	= 0;
}

static void
shacheck_progress(struct shacheck *m, unsigned char *prefix)
{
  int	i;

  printf("\r");
  for (i=0; i<m->variant; i++)
    printf("%02x", prefix[i]);
  fflush(stdout);
}

static void
shacheck_create(struct shacheck *m, char **argv)
{
  unsigned char	prefix[SHACHECK_VARIANT_MAX];
  int		best;

  /* the inputs are assumed to be sorted on the HASH */

  memset(prefix, 0, sizeof prefix);	/* shutup compiler	*/
  for (shacheck_inputs_open(m, argv); (best=shacheck_input_best(m, 0, m->input_count))>=0; shacheck_input_read(m, best))
    {
      /* find the lowest HASH	*/
      xDP(("() best=%d", best));
      if (best<0)
        break;

      m->input	= &m->inputs[best];

      if (m->hashlen != m->input->len)
        OOPS("%s:%d: all files must have the same HASH length: %d", m->input->name, m->input->line, m->hashlen);

      /* do we need to switch cache file?	*/
      if (!m->fd || memcmp(prefix, m->input->buf, m->variant))
        {
          shacheck_hash_close(m);

          shacheck_mk_hashname(m, m->input->buf, 0);	/* this returns the directory in global "hashname" */
          if (!isdir(m->hashname) && mkdir(m->hashname, 0755))
            OOPS("%s: cannot create directory", m->hashname);

          if ((m->fd = fopen(shacheck_mk_hashname(m, m->input->buf, 1), "wb")) == NULL)
            OOPS("%s: cannot create file", m->hashname);

          /* write file header	*/
          shacheck_mk_magic(m);
          fprintf(m->fd, "%s%c", m->magic, m->hashlen);

          shacheck_progress(m, prefix);
        }
      /* just write out the SHAs with the first two bytes removed,
       * as those are encoded in the XX/XX.hash already
       * (this is a form of compression).
       */
      fwrite(m->input->buf + m->variant, m->hashlen - m->variant, 1, m->fd);

    }
  shacheck_hash_close(m);
}

/* sets:
 * offset  = offset of data
 * total   = total file length
 * hashlen = size of hash (first 2 bytes are in the filename)
 */
static void
shacheck_hash_open(struct shacheck *m, unsigned char *hash)
{
  int	c;
  char	buf[SHACHECK_MAGIC_BUF];

  if (m->fd)
    {
      OOPS("internal error: shacheck_hash_close() was not called");
      shacheck_hash_close(m);	/* not reached, but would be proper mititgagion	*/
    }

  shacheck_mk_hashname(m, hash, 1);
  if ((m->fd = fopen(m->hashname, "rb")) == NULL)
    {
      WARN(m, 0, "%s not found: %s", m->hashname);
      return;
    }

  shacheck_mk_magic(m);

  if (1 != fread(buf, m->hash_offset, 1, m->fd))
    OOPS("%s: corrupt file, cannot read header", m->hashname);
  if (memcmp(buf, m->magic, m->hash_offset-1))
    OOPS("%s: corrupt file, wrong magic in header", m->hashname);

  c	= (unsigned char)buf[m->hash_offset-1];
  if (!m->hashlen)
    m->hashlen = c>=SHACHECK_MINHASH && c<=SHACHECK_MAXHASH ? c : -1;	/* -1 = OOPS, as c>=0	*/
  if (c != m->hashlen)
    OOPS("%s: corrupt file, hashlen %d (expected [%d..%d])", m->hashname, c, m->hashlen>0 ? m->hashlen : SHACHECK_MINHASH, m->hashlen>0 ? m->hashlen : SHACHECK_MAXHASH);

  /* Check the file size	*/

  if (fseek(m->fd, 0, SEEK_END))
    OOPS("%s: cannot seek to EOF", m->hashname);

  m->hash_filesize	= ftell(m->fd);
  if ((m->hash_filesize - m->hash_offset) % (m->hashlen - m->variant))
    OOPS("%s: corrupt file, %ld not divisible by %d", m->hashname, (m->hash_filesize - m->hash_offset), (m->hashlen - m->variant));
}

static void
shacheck_hash_seek(struct shacheck *m, long off)
{
  if (fseek(m->fd, off, SEEK_SET))
    OOPS("%s: cannot seek to %l", m->hashname, off);
}

static void
shacheck_check_one(struct shacheck *m, char *line)
{
  unsigned char	hash[SHACHECK_MAXHASH];
  int		i, c;

  char		buf[BUFSIZ];
  long		size, entries, min, max;

  xDP(("(%s)", line));

  line	= trims(line);
  if (!*line)
    return;

  for (i=0; i<SHACHECK_MAXHASH && (c=hexbyte(line+i+i))>=0; i++)
    if (i<SHACHECK_MAXHASH)
      hash[i]	= c;
    else
      {
        WARN(m, 0, "hashlength %d not in [%d..%d]: %s", i+1, SHACHECK_MINHASH, SHACHECK_MAXHASH, line);
        return;
      }

  if (line[i+i] && !isspace(line[i+i]))
    {
      WARN(m, 0, "malformed: %s", line);
      return;
    }
  if (i<SHACHECK_MINHASH)
    {
      WARN(m, 0, "hashlength %d not in [%d..%d]: %s", i, SHACHECK_MINHASH, SHACHECK_MAXHASH, line);
      return;
    }

  /* Always open and close the hashfile, to keep things simple.
   *
   * This might only be inefficient in case of sorted input.
   * However this is meant for fast single lookups for now.
   */
  shacheck_hash_open(m, hash);

  if (i != m->hashlen)
    {
      WARN(m, 0, "wrong hashlength %d (expected %d): %s", i, m->hashlen, line);
      return;
    }

  size		= m->hashlen - m->variant;
  entries	= (m->hash_filesize - m->hash_offset) / size;

  min		= 0;
  max		= entries;

  for (;;)
    {
      int	ent;

      if (min>=max)
        {
          printf("NOTFOUND: %s\n", line);
          break;
        }
      ent	= (min+max)/2;
      shacheck_hash_seek(m, m->hash_offset + size * ent);
      if (1 != fread(buf, size, 1, m->fd))
        OOPS("%s: read error entry %ld", m->hashname, ent);	/* whatever this means on read	*/
      i	= memcmp(hash + m->variant, buf, size);
      if (!i)
        {
          printf("FOUND: %s\n", line);
          m->err	= 0;
          break;
        }
      if (i<0)
        max	= ent;
      else
        min	= ent+1;
    }

  if (ferror(m->fd) || fclose(m->fd))
    OOPS("%s: read error", m->hashname);	/* whatever this means on read	*/
}

static void
shacheck_check(struct shacheck *m, char **argv)
{
  char	buf[BUFSIZ];

  m->err = 2;
  if (!*argv)
    while (fgets(buf, (sizeof buf)-1, stdin))
      shacheck_check_one(m, buf);
  else
    while (*argv)
      shacheck_check_one(m, *argv++);
}

/* diagnostics	*/

static void
shacheck_dump(struct shacheck *m, char **argv)
{
  unsigned char	hash[256];
  int		min, max, i, j;

  min	= 0;
  max	= 65535;

  if (*argv)
    min	= atoi(*argv++);
  if (*argv)
    max	= atoi(*argv++);
  if (*argv)
    OOPS("too many arguments, .. dump min max");
  if (min<0)
    {
      min	= 0;
      WARN(m, 0, "minimum set to %d", min);
    }
  if (max>65535)
    {
      max	= 65535;
      WARN(m, 0, "max set to %d", max);
    }

  for (i=min; i<=max; i++)
    {
      long	pos;
      int	step;

      fprintf(stderr, "\r%04x", i); fflush(stderr);

      hash[0]	= i>>8;
      hash[1]	= i;

      shacheck_hash_open(m, hash);
      shacheck_hash_seek(m, m->hash_offset);
      step	= m->hashlen - m->variant;
      for (pos = m->hash_offset; pos < m->hash_filesize; pos += step)
        {
          if (1 != fread(hash + m->variant, step, 1, m->fd))
            OOPS("%s: read error", m->hashname);
          for (j=0; j < m->hashlen; j++)
            printf("%02X", hash[j]);
          printf("\n");
        }
      shacheck_hash_close(m);
    }
}

static int
shacheck_variant(struct shacheck *m, const char *variant)
{
  if (!strcmp(variant, "2"))
    return m->variant = SHACHECK_VARIANT2;
  if (!strcmp(variant, "3"))
    return m->variant = SHACHECK_VARIANT3;

  m->variant	= SHACHECK_VARIANT_DEF;
  return 0;
}

static void
usage(char **argv)
{
  OOPS("Usage: %s datadir [variant] create|check|dump args..", argv[0]);
}

static struct shacheck shacheck = { 0 };

int
main(int argc, char **argv)
{
  struct shacheck	*m = &shacheck;
  const char		*cmd;
  int			argn;

  if (argc<3)
    usage(argv);

  strncpy(m->dir, argv[1], sizeof m->dir);
  if (m->dir[(sizeof m->dir)-SHACHECK_PATH_NEEDED])
    OOPS("%s: name too long", argv[1]);

  if (!isdir(m->dir))
    OOPS("%s: not a directory", m->dir);

  argn	= shacheck_variant(m, argv[2]) ? 3 : 4;
  cmd	= argv[argn++];

  if (!strcmp(cmd, "create"))
    shacheck_create(m, argv+argn);
  else if (!strcmp(cmd, "check"))
    shacheck_check(m, argv+argn);
  else if (!strcmp(cmd, "dump"))
    shacheck_dump(m, argv+argn);
  else
    usage(argv);

  return m->err;
}

