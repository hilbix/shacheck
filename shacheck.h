/* This Works is placed under the terms of the Copyright Less License,
 * see file COPYRIGHT.CLL.  USE AT OWN RISK, ABSOLUTELY NO WARRANTY.
 */

#define SHACHECK_ZMQ_DEFAULT    "tcp://127.0.0.1:1354"  /* 0x54a        */

void OOPS(const char *s, ...);

#define	ZMQ_STR2(x)	#x
#define	ZMQ_STR1(x)	ZMQ_STR2(x)
#define	ZMQ_FATAL(x)	do { if (x) OOPS("FATAL %s:%d:%s: %s", __FILE__, ZMQ_STR1(__LINE__), __FUNCTION__, #x); } while (0)

static void
ZMQ_bind(void *socket, const char *dest)
{
  ZMQ_FATAL(!socket || !dest);
  if (zmq_bind(socket, dest))
    OOPS("zmq_bind failed: %s", dest);
}

static void
ZMQ_connect(void *socket, const char *dest)
{
  ZMQ_FATAL(!socket || !dest);
  if (zmq_connect(socket, dest))
    OOPS("zmq_connect failed: %s", dest);
}

static size_t
ZMQ_in(void *socket, void *buf, size_t len)
{
  int	n;

  ZMQ_FATAL(!socket || !buf || len<1);
  n	= zmq_recv(socket, buf, len-1, 0);
  if (n<=0)
    OOPS("zmq_recv failed: %d", n);
  ((char *)buf)[n]	= 0;
  return n;
}

static void
ZMQ_out(void *socket, void *buf, size_t len)
{
  int	n;

  ZMQ_FATAL(!socket || !buf || len<1);
  n	= zmq_send(socket, buf, len, 0);
  if (n!=len)
    OOPS("zmq_send failed: %d", n);
}

