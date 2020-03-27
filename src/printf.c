#include "ether/ether.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

void
eth_vfprintf(FILE *out, const char *fmt, va_list arg)
{
  if (*fmt == 0) return;

  char *p = strchr(fmt, '~');
  if (p && (p[1] == 'w' || p[1] == 'd'))
  {
    if (p != fmt)
    {
      int n = p - fmt;
      char buf[n + 1];
      memcpy(buf, fmt, n);
      buf[n] = 0;
      vfprintf(out, buf, arg);
    }

    eth_t x = va_arg(arg, eth_t);
    switch (p[1])
    {
      case 'w':
        eth_write(x, out);
        break;

      case 'd':
        eth_display(x, out);
        break;
    }

    eth_vfprintf(out, p + 2, arg);
  }
  else
    vfprintf(out, fmt, arg);
}

void
eth_vprintf(const char *fmt, va_list arg)
{
  eth_vfprintf(stdout, fmt, arg);
}

void
eth_fprintf(FILE *out, const char *fmt, ...)
{
  va_list arg;
  va_start(arg, fmt);
  eth_vfprintf(out, fmt, arg);
  va_end(arg);
}

void
eth_printf(const char *fmt, ...)
{
  va_list arg;
  va_start(arg, fmt);
  eth_vprintf(fmt, arg);
  va_end(arg);
}

