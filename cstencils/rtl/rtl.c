/* rtl.c - own runtime library for programs compiled by cmini (no glibc).
   Freestanding C; syscalls on x86-64 Linux.  Provides the 33 external
   symbols used by the cstencils objects (see notes in the report). */

typedef unsigned long size_t;
typedef long ssize_t;

#define NULL ((void *) 0)

/* ---------------------------------------------------------------- syscalls */

static long sys3 (long n, long a, long b, long c)
{
   long ret;
   __asm__ volatile ("syscall" : "=a" (ret)
                     : "a" (n), "D" (a), "S" (b), "d" (c)
                     : "rcx", "r11", "memory");
   return ret;
}

static long sys6 (long n, long a, long b, long c, long d, long e, long f)
{
   long ret;
   register long r10 __asm__ ("r10") = d;
   register long r8  __asm__ ("r8")  = e;
   register long r9  __asm__ ("r9")  = f;
   __asm__ volatile ("syscall" : "=a" (ret)
                     : "a" (n), "D" (a), "S" (b), "d" (c), "r" (r10), "r" (r8), "r" (r9)
                     : "rcx", "r11", "memory");
   return ret;
}

static void sys_exit_group (int code)
{
   for (;;)
      sys3 (231, code, 0, 0);
}

/* ---------------------------------------------------------- start, exit */

int    rtl_argc;
char ** rtl_argv;
char ** rtl_envp;

typedef void (* atexit_fn) (void);
static atexit_fn exit_fns [32];
static int       exit_count = 0;

extern int main (int argc, char ** argv);

void exit (int code)
{
   while (exit_count > 0)
   {
      exit_count--;
      exit_fns [exit_count] ();
   }
   sys_exit_group (code);
}

int atexit (atexit_fn fn)
{
   if (exit_count >= 32)
      return -1;
   exit_fns [exit_count++] = fn;
   return 0;
}

void abort (void)
{
   static const char msg [] = "abort\n";
   sys3 (1, 2, (long) msg, sizeof (msg) - 1);
   sys_exit_group (134);
}

void rtl_start (long * sp)
{
   int argc = (int) sp [0];
   char ** argv = (char **) (sp + 1);
   char ** envp = argv + argc + 1;
   rtl_argc = argc;
   rtl_argv = argv;
   rtl_envp = envp;
   exit (main (argc, argv));
}

__asm__ (
   ".text\n"
   ".globl _start\n"
   "_start:\n"
   "   xor %rbp, %rbp\n"
   "   mov %rsp, %rdi\n"
   "   and $-16, %rsp\n"
   "   call rtl_start\n"
   "   hlt\n");

/* ------------------------------------------------------------- strings */

void * memcpy (void * dst, const void * src, size_t n)
{
   unsigned char * d = dst;
   const unsigned char * s = src;
   while (n-- > 0)
      *d++ = *s++;
   return dst;
}

void * memmove (void * dst, const void * src, size_t n)
{
   unsigned char * d = dst;
   const unsigned char * s = src;
   if (d < s)
      while (n-- > 0)
         *d++ = *s++;
   else
      while (n > 0)
      {
         n--;
         d [n] = s [n];
      }
   return dst;
}

void * memset (void * dst, int c, size_t n)
{
   unsigned char * d = dst;
   while (n-- > 0)
      *d++ = (unsigned char) c;
   return dst;
}

int memcmp (const void * a, const void * b, size_t n)
{
   const unsigned char * x = a;
   const unsigned char * y = b;
   for (; n > 0; n--, x++, y++)
      if (*x != *y)
         return *x < *y ? -1 : 1;
   return 0;
}

size_t strlen (const char * s)
{
   size_t n = 0;
   while (s [n] != 0)
      n++;
   return n;
}

int strcmp (const char * a, const char * b)
{
   while (*a != 0 && *a == *b)
   {
      a++;
      b++;
   }
   return (unsigned char) *a - (unsigned char) *b;
}

int strncmp (const char * a, const char * b, size_t n)
{
   for (; n > 0; n--, a++, b++)
   {
      if (*a != *b)
         return (unsigned char) *a - (unsigned char) *b;
      if (*a == 0)
         return 0;
   }
   return 0;
}

char * strchr (const char * s, int c)
{
   for (;; s++)
   {
      if (*s == (char) c)
         return (char *) s;
      if (*s == 0)
         return NULL;
   }
}

char * strstr (const char * hay, const char * needle)
{
   size_t n = strlen (needle);
   if (n == 0)
      return (char *) hay;
   for (; *hay != 0; hay++)
      if (strncmp (hay, needle, n) == 0)
         return (char *) hay;
   return NULL;
}

char * getenv (const char * name)
{
   size_t n = strlen (name);
   char ** e;
   if (rtl_envp == NULL)
      return NULL;
   for (e = rtl_envp; *e != NULL; e++)
      if (strncmp (*e, name, n) == 0 && (*e) [n] == '=')
         return *e + n + 1;
   return NULL;
}

/* ------------------------------------------------------------- memory */

/* bump allocator from anonymous mmap chunks; every block has a size header */

typedef struct block_hdr { size_t size; size_t pad; } block_hdr;

static unsigned char * heap_cur = NULL;
static unsigned char * heap_end = NULL;

static void * heap_alloc (size_t n)
{
   size_t need = sizeof (block_hdr) + ((n + 15) & ~(size_t) 15);
   block_hdr * h;

   if (heap_cur == NULL || (size_t) (heap_end - heap_cur) < need)
   {
      size_t chunk = need > (1UL << 24) ? need : (1UL << 24);
      long p = sys6 (9, 0, (long) chunk, 3, 0x22, -1, 0);   /* mmap RW, private anon */
      if (p < 0 && p > -4096)
         return NULL;
      heap_cur = (unsigned char *) p;
      heap_end = heap_cur + chunk;
   }
   h = (block_hdr *) heap_cur;
   heap_cur += need;
   h->size = n;
   return h + 1;
}

void * malloc (size_t n)
{
   return heap_alloc (n);
}

void * calloc (size_t n, size_t m)
{
   size_t total = n * m;
   void * p = heap_alloc (total);
   if (p != NULL)
      memset (p, 0, total);
   return p;
}

void * realloc (void * p, size_t n)
{
   void * q;
   size_t old;
   if (p == NULL)
      return heap_alloc (n);
   old = ((block_hdr *) p - 1)->size;
   if (n <= old)
      return p;
   q = heap_alloc (n);
   if (q != NULL)
      memcpy (q, p, old);
   return q;
}

void free (void * p)
{
   (void) p;   /* bump allocator: memory is released at exit */
}

/* ------------------------------------------------------------- FILE */

typedef struct _FILE
{
   int fd;
   int err;
   int eof;
} FILE;

static FILE stdin_obj  = { 0, 0, 0 };
static FILE stdout_obj = { 1, 0, 0 };
static FILE stderr_obj = { 2, 0, 0 };

FILE * stdin  = &stdin_obj;
FILE * stdout = &stdout_obj;
FILE * stderr = &stderr_obj;

static size_t write_all (int fd, const char * buf, size_t n)
{
   size_t done = 0;
   while (done < n)
   {
      long r = sys3 (1, fd, (long) (buf + done), (long) (n - done));
      if (r <= 0)
         break;
      done += (size_t) r;
   }
   return done;
}

ssize_t write (int fd, const void * buf, size_t n)
{
   return (ssize_t) write_all (fd, buf, n);
}

size_t fwrite (const void * buf, size_t size, size_t n, FILE * f)
{
   size_t bytes = size * n;
   size_t done = write_all (f->fd, buf, bytes);
   return done / (size ? size : 1);
}

size_t fread (void * buf, size_t size, size_t n, FILE * f)
{
   size_t total = size * n;
   size_t done = 0;
   while (done < total)
   {
      long r = sys3 (0, f->fd, (long) ((char *) buf + done), (long) (total - done));
      if (r <= 0)
      {
         f->eof = 1;
         break;
      }
      done += (size_t) r;
   }
   return done / (size ? size : 1);
}

int fflush (FILE * f)
{
   (void) f;
   return 0;
}

int setvbuf (FILE * f, char * buf, int mode, size_t size)
{
   (void) f; (void) buf; (void) mode; (void) size;
   return 0;
}

FILE * fdopen (int fd, const char * mode)
{
   FILE * f;
   (void) mode;
   f = malloc (sizeof (FILE));
   if (f != NULL)
   {
      f->fd = fd;
      f->err = 0;
      f->eof = 0;
   }
   return f;
}

FILE * fopen (const char * name, const char * mode)
{
   /* r: O_RDONLY, w: O_WRONLY|O_CREAT|O_TRUNC, a: O_WRONLY|O_CREAT|O_APPEND */
   long flags = 0;
   long fd;
   if (mode [0] == 'w')
      flags = 01 | 0100 | 01000;
   else if (mode [0] == 'a')
      flags = 01 | 0100 | 02000;
   fd = sys3 (2, (long) name, flags, 0644);
   if (fd < 0)
      return NULL;
   return fdopen ((int) fd, mode);
}

int fclose (FILE * f)
{
   int r;
   if (f == stdin || f == stdout || f == stderr)
      return 0;
   r = (int) sys3 (3, f->fd, 0, 0);
   free (f);
   return r < 0 ? -1 : 0;
}

/* ------------------------------------------------------------- printf */

#define MAXNUM 64

typedef struct { char * buf; size_t size; size_t len; FILE * f; int to_file; } out_t;

static void out_char (out_t * o, char c)
{
   if (o->to_file)
   {
      write_all (o->f->fd, &c, 1);
      o->len++;
      return;
   }
   if (o->len + 1 < o->size)
      o->buf [o->len] = c;
   o->len++;
}

static void out_str (out_t * o, const char * s, int width, int left)
{
   size_t n = strlen (s);
   int pad = width > (int) n ? width - (int) n : 0;
   int i;
   if (! left)
      for (i = 0; i < pad; i++)
         out_char (o, ' ');
   for (i = 0; i < (int) n; i++)
      out_char (o, s [i]);
   if (left)
      for (i = 0; i < pad; i++)
         out_char (o, ' ');
}

static void fmt_unsigned (unsigned long v, int base, int upper, char * out)
{
   char tmp [MAXNUM];
   int n = 0;
   int i;
   const char * digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
   do
   {
      tmp [n++] = digits [v % base];
      v /= base;
   }
   while (v != 0);
   for (i = 0; i < n; i++)
      out [i] = tmp [n - 1 - i];
   out [n] = 0;
}

/* simple %g / %f: fixed notation with up to 6 significant digits */
static void fmt_double (double v, char * out)
{
   char tmp [MAXNUM];
   int neg = v < 0;
   unsigned long ip;
   double frac;
   int n = 0, i, digits = 6;
   if (neg)
      v = -v;
   ip = (unsigned long) v;
   frac = v - (double) ip;
   fmt_unsigned (ip, 10, 0, tmp);
   n = (int) strlen (tmp);
   out [0] = 0;
   if (neg)
      out [0] = '-', out [1] = 0;
   memcpy (out + (neg ? 1 : 0), tmp, (size_t) n + 1);
   if (digits > 0)
   {
      int len = (int) strlen (out);
      out [len++] = '.';
      for (i = 0; i < digits; i++)
      {
         frac *= 10;
         out [len++] = (char) ('0' + (int) frac);
         frac -= (int) frac;
      }
      out [len] = 0;
   }
}

static int vformat (out_t * o, const char * fmt, __builtin_va_list ap)
{
   char num [MAXNUM];
   while (*fmt != 0)
   {
      int width = 0, left = 0, longs = 0;
      if (*fmt != '%')
      {
         out_char (o, *fmt++);
         continue;
      }
      fmt++;
      if (*fmt == '-')
      {
         left = 1;
         fmt++;
      }
      while (*fmt >= '0' && *fmt <= '9')
         width = width * 10 + (*fmt++ - '0');
      while (*fmt == 'l' || *fmt == 'h' || *fmt == 'z')
      {
         if (*fmt == 'l' || *fmt == 'z')
            longs++;
         fmt++;
      }
      switch (*fmt)
      {
         case 'd':
         case 'i':
         {
            long v = longs ? __builtin_va_arg (ap, long) : (long) __builtin_va_arg (ap, int);
            unsigned long u = v < 0 ? (unsigned long) (-(v + 1)) + 1 : (unsigned long) v;
            fmt_unsigned (u, 10, 0, num);
            if (v < 0)
            {
               char tmp [MAXNUM];
               tmp [0] = '-';
               memcpy (tmp + 1, num, strlen (num) + 1);
               memcpy (num, tmp, strlen (tmp) + 1);
            }
            out_str (o, num, width, left);
            break;
         }
         case 'u':
         case 'x':
         case 'X':
         {
            unsigned long v = longs ? __builtin_va_arg (ap, unsigned long)
                                    : (unsigned long) __builtin_va_arg (ap, unsigned int);
            fmt_unsigned (v, *fmt == 'u' ? 10 : 16, *fmt == 'X', num);
            out_str (o, num, width, left);
            break;
         }
         case 'p':
         {
            unsigned long v = (unsigned long) __builtin_va_arg (ap, void *);
            fmt_unsigned (v, 16, 0, num);
            out_str (o, "0x", 0, 0);
            out_str (o, num, width, left);
            break;
         }
         case 'c':
         {
            char c [2];
            c [0] = (char) __builtin_va_arg (ap, int);
            c [1] = 0;
            out_str (o, c, width, left);
            break;
         }
         case 's':
         {
            const char * s = __builtin_va_arg (ap, const char *);
            out_str (o, s ? s : "(null)", width, left);
            break;
         }
         case 'f':
         case 'g':
         case 'e':
         {
            fmt_double (__builtin_va_arg (ap, double), num);
            out_str (o, num, width, left);
            break;
         }
         case '%':
            out_char (o, '%');
            break;
         case 0:
            return (int) o->len;
         default:
            out_char (o, '%');
            out_char (o, *fmt);
            break;
      }
      fmt++;
   }
   return (int) o->len;
}

int vsnprintf (char * buf, size_t size, const char * fmt, __builtin_va_list ap)
{
   out_t o;
   int r;
   o.buf = buf; o.size = size; o.len = 0; o.f = NULL; o.to_file = 0;
   r = vformat (&o, fmt, ap);
   if (size > 0)
      buf [o.len < size ? o.len : size - 1] = 0;
   return r;
}

int snprintf (char * buf, size_t size, const char * fmt, ...)
{
   __builtin_va_list ap;
   int r;
   __builtin_va_start (ap, fmt);
   r = vsnprintf (buf, size, fmt, ap);
   __builtin_va_end (ap);
   return r;
}

int fprintf (FILE * f, const char * fmt, ...)
{
   __builtin_va_list ap;
   out_t o;
   int r;
   o.buf = NULL; o.size = 0; o.len = 0; o.f = f; o.to_file = 1;
   __builtin_va_start (ap, fmt);
   r = vformat (&o, fmt, ap);
   __builtin_va_end (ap);
   return r;
}

int dprintf (int fd, const char * fmt, ...)
{
   __builtin_va_list ap;
   FILE tmp;
   out_t o;
   int r;
   tmp.fd = fd; tmp.err = 0; tmp.eof = 0;
   o.buf = NULL; o.size = 0; o.len = 0; o.f = &tmp; o.to_file = 1;
   __builtin_va_start (ap, fmt);
   r = vformat (&o, fmt, ap);
   __builtin_va_end (ap);
   return r;
}

int fputs (const char * s, FILE * f)
{
   size_t n = strlen (s);
   return write_all (f->fd, s, n) == n ? 0 : -1;
}

/* ------------------------------------------------------------- strtod */

double strtod (const char * s, char ** end)
{
   double v = 0, scale = 1;
   int neg = 0, expo = 0, frac = 0;
   const char * p = s;
   while (*p == ' ' || *p == '\t' || *p == '\n')
      p++;
   if (*p == '-' || *p == '+')
      neg = (*p++ == '-');
   while (*p >= '0' && *p <= '9')
      v = v * 10 + (*p++ - '0');
   if (*p == '.')
   {
      p++;
      while (*p >= '0' && *p <= '9')
      {
         v = v * 10 + (*p++ - '0');
         scale *= 10;
         frac = 1;
      }
   }
   if (*p == 'e' || *p == 'E')
   {
      int eneg = 0, e = 0;
      const char * q = p + 1;
      if (*q == '-' || *q == '+')
         eneg = (*q++ == '-');
      if (*q >= '0' && *q <= '9')
      {
         while (*q >= '0' && *q <= '9')
            e = e * 10 + (*q++ - '0');
         expo = eneg ? -e : e;
         p = q;
      }
   }
   while (expo > 0) { v *= 10; expo--; }
   while (expo < 0) { v /= 10; expo++; }
   if (frac)
      v /= scale;
   if (end != NULL)
      *end = (char *) p;
   return neg ? -v : v;
}

/* ------------------------------------------------------------- setjmp */

/* longjmp is used by cmini error recovery only when a recovery point is set;
   the cstencils main does not set one, so reaching this is a bug */
void longjmp (void * env, int val)
{
   (void) env; (void) val;
   static const char msg [] = "longjmp: not supported by own RTL\n";
   sys3 (1, 2, (long) msg, sizeof (msg) - 1);
   sys_exit_group (70);
}
